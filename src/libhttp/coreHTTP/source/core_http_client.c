/*
 * coreHTTP v2.0.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file core_http_client.c
 * @brief Implements the user-facing functions in core_http_client.h.
 */

#include <assert.h>
#include <limits.h>
#include <string.h>

#include "core_http_client.h"
#include "core_http_client_private.h"

/*-----------------------------------------------------------*/

/**
 * @brief When #HTTPResponse_t.getTime is set to NULL in #HTTPClient_Send then
 * this function will replace that field.
 *
 * @return This function always returns zero.
 */
static uint32_t getZeroTimestampMs( void );

/**
 * @brief Send HTTP bytes over the transport send interface.
 *
 * @param[in] pTransport Transport interface.
 * @param[in] getTimestampMs Function to retrieve a timestamp in milliseconds.
 * @param[in] pData HTTP request data to send.
 * @param[in] dataLen HTTP request data length.
 *
 * @return #HTTPSuccess if successful. If there was a network error or less
 * bytes than what were specified were sent, then #HTTPNetworkError is
 * returned.
 */
static HTTPStatus_t sendHttpData( const TransportInterface_t * pTransport,
                                  HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                  const uint8_t * pData,
                                  size_t dataLen );

/**
 * @brief Send the HTTP headers over the transport send interface.
 *
 * @param[in] pTransport Transport interface.
 * @param[in] getTimestampMs Function to retrieve a timestamp in milliseconds.
 * @param[in] pRequestHeaders Request headers to send, it includes the buffer
 * and length.
 * @param[in] reqBodyLen The length of the request body to be sent. This is
 * used to generated a Content-Length header.
 * @param[in] sendFlags Application provided flags to #HTTPClient_Send.
 *
 * @return #HTTPSuccess if successful. If there was a network error or less
 * bytes than what were specified were sent, then #HTTPNetworkError is
 * returned.
 */
static HTTPStatus_t sendHttpHeaders( const TransportInterface_t * pTransport,
                                     HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                     HTTPRequestHeaders_t * pRequestHeaders,
                                     size_t reqBodyLen,
                                     uint32_t sendFlags );

/**
 * @brief Adds the Content-Length header field and value to the
 * @p pRequestHeaders.
 *
 * @param[in] pRequestHeaders Request header buffer information.
 * @param[in] contentLength The Content-Length header value to write.
 *
 * @return #HTTPSuccess if successful. If there was insufficient memory in the
 * application buffer, then #HTTPInsufficientMemory is returned.
 */
static HTTPStatus_t addContentLengthHeader( HTTPRequestHeaders_t * pRequestHeaders,
                                            size_t contentLength );

/**
 * @brief Send the HTTP body over the transport send interface.
 *
 * @param[in] pTransport Transport interface.
 * @param[in] getTimestampMs Function to retrieve a timestamp in milliseconds.
 * @param[in] pRequestBodyBuf Request body buffer.
 * @param[in] reqBodyBufLen Length of the request body buffer.
 *
 * @return #HTTPSuccess if successful. If there was a network error or less
 * bytes than what was specified were sent, then #HTTPNetworkError is
 * returned.
 */
static HTTPStatus_t sendHttpBody( const TransportInterface_t * pTransport,
                                  HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                  const uint8_t * pRequestBodyBuf,
                                  size_t reqBodyBufLen );

/**
 * @brief A strncpy replacement with HTTP header validation.
 *
 * This function checks for `\r` and `\n` in the @p pSrc while copying.
 * This function checks for `:` in the @p pSrc, if @p isField is set to 1.
 *
 * @param[in] pDest The destination buffer to copy to.
 * @param[in] pSrc The source buffer to copy from.
 * @param[in] len The length of @p pSrc to copy to pDest.
 * @param[in] isField Set to 0 to indicate that @p pSrc is a field. Set to 1 to
 * indicate that @p pSrc is a value.
 *
 * @return @p pDest if the copy was successful, NULL otherwise.
 */
static char * httpHeaderStrncpy( char * pDest,
                                 const char * pSrc,
                                 size_t len,
                                 uint8_t isField );

static int addSizeTNoOverflow( size_t a, size_t b, size_t * pResult );

/**
 * @brief Write header based on parameters. This method also adds a trailing
 * "\r\n". If a trailing "\r\n" already exists in the HTTP header, this method
 * backtracks in order to write over it and updates the length accordingly.
 *
 * @param[in] pRequestHeaders Request header buffer information.
 * @param[in] pField The ISO 8859-1 encoded header field name to write.
 * @param[in] fieldLen The byte length of the header field name.
 * @param[in] pValue The ISO 8859-1 encoded header value to write.
 * @param[in] valueLen The byte length of the header field value.
 *
 * @return #HTTPSuccess if successful. If there was insufficient memory in the
 * application buffer, then #HTTPInsufficientMemory is returned.
 */
static HTTPStatus_t addHeader( HTTPRequestHeaders_t * pRequestHeaders,
                               const char * pField,
                               size_t fieldLen,
                               const char * pValue,
                               size_t valueLen );

/**
 * @brief Add the byte range request header to the request headers store in
 * #HTTPRequestHeaders_t.pBuffer once all the parameters are validated.
 *
 * @param[in] pRequestHeaders Request header buffer information.
 * @param[in] rangeStartOrlastNbytes Represents either the starting byte
 * for a range OR the last N number of bytes in the requested file.
 * @param[in] rangeEnd The ending range for the requested file. For end of file
 * byte in Range Specifications 2. and 3., #HTTP_RANGE_REQUEST_END_OF_FILE
 * should be passed.
 *
 * @return #HTTPSuccess if successful. If there was insufficient memory in the
 * application buffer, then #HTTPInsufficientMemory is returned.
 */
static HTTPStatus_t addRangeHeader( HTTPRequestHeaders_t * pRequestHeaders,
                                    int32_t rangeStartOrlastNbytes,
                                    int32_t rangeEnd );

/**
 * @brief Get the status of the HTTP response given the parsing state and how
 * much data is in the response buffer.
 *
 * @param[in] parsingState State of the parsing on the HTTP response.
 * @param[in] totalReceived The amount of network data received in the response
 * buffer.
 * @param[in] responseBufferLen The length of the response buffer.
 *
 * @return Returns #HTTPSuccess if the parsing state is complete. If
 * the parsing state denotes it never started, then return #HTTPNoResponse. If
 * the parsing state is incomplete, then if the response buffer is not full
 * #HTTPPartialResponse is returned. If the parsing state is incomplete, then
 * if the response buffer is full #HTTPInsufficientMemory is returned.
 */
static HTTPStatus_t getFinalResponseStatus( HTTPParsingState_t parsingState,
                                            size_t totalReceived,
                                            size_t responseBufferLen );

/**
 * @brief Receive the HTTP response from the network and parse it.
 *
 * @param[in] pTransport Transport interface.
 * @param[in] pResponse Response message to receive data from the network.
 * @param[in] pRequestHeaders Request headers for the corresponding HTTP request.
 *
 * @return Returns #HTTPSuccess if successful. #HTTPNetworkError for a transport
 * receive error. Please see #parseHttpResponse and #getFinalResponseStatus for
 * other statuses returned.
 */
static HTTPStatus_t receiveAndParseHttpResponse( const TransportInterface_t * pTransport,
                                                 HTTPResponse_t * pResponse,
                                                 const HTTPRequestHeaders_t * pRequestHeaders );

/**
 * @brief Send the HTTP request over the network.
 *
 * @param[in] pTransport Transport interface.
 * @param[in] getTimestampMs Function to retrieve a timestamp in milliseconds.
 * @param[in] pRequestHeaders Request headers to send over the network.
 * @param[in] pRequestBodyBuf Request body buffer to send over the network.
 * @param[in] reqBodyBufLen Length of the request body buffer.
 * @param[in] sendFlags Application provided flags to #HTTPClient_Send.
 *
 * @return Returns #HTTPSuccess if successful. Please see #sendHttpHeaders and
 * #sendHttpBody for other statuses returned.
 */
static HTTPStatus_t sendHttpRequest( const TransportInterface_t * pTransport,
                                     HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                     HTTPRequestHeaders_t * pRequestHeaders,
                                     const uint8_t * pRequestBodyBuf,
                                     size_t reqBodyBufLen,
                                     uint32_t sendFlags );

/**
 * @brief Converts an integer value to its ASCII representation in the passed
 * buffer.
 *
 * @param[in] value The value to convert to ASCII.
 * @param[out] pBuffer The buffer to store the ASCII representation of the
 * integer.
 * @param[in] bufferLength The length of pBuffer.
 *
 * @return Returns the number of bytes written to @p pBuffer.
 */
static uint8_t convertInt32ToAscii( int32_t value,
                                    char * pBuffer,
                                    size_t bufferLength );

/**
 * @brief This method writes the request line (first line) of the HTTP Header
 * into #HTTPRequestHeaders_t.pBuffer and updates length accordingly.
 *
 * @param pRequestHeaders Request header buffer information.
 * @param pMethod The HTTP request method e.g. "GET", "POST", "PUT", or "HEAD".
 * @param methodLen The byte length of the request method.
 * @param pPath The Request-URI to the objects of interest, e.g. "/path/to/item.txt".
 * @param pathLen The byte length of the request path.
 *
 * @return #HTTPSuccess if successful. If there was insufficient memory in the
 * application buffer, then #HTTPInsufficientMemory is returned.
 */
static HTTPStatus_t writeRequestLine( HTTPRequestHeaders_t * pRequestHeaders,
                                      const char * pMethod,
                                      size_t methodLen,
                                      const char * pPath,
                                      size_t pathLen );

/**
 * @brief Find the specified header field in the response buffer.
 *
 * @param[in] pBuffer The response buffer to parse.
 * @param[in] bufferLen The length of the response buffer to parse.
 * @param[in] pField The header field to search for.
 * @param[in] fieldLen The length of pField.
 * @param[out] pValueLoc The location of the the header value found in pBuffer.
 * @param[out] pValueLen The length of pValue.
 *
 * @return One of the following:
 * - #HTTPSuccess when header is found in the response.
 * - #HTTPHeaderNotFound if requested header is not found in response.
 * - #HTTPInvalidResponse if passed response is invalid for parsing.
 * - #HTTPParserInternalError for any parsing errors.
 */
static HTTPStatus_t findHeaderInResponse( const uint8_t * pBuffer,
                                          size_t bufferLen,
                                          const char * pField,
                                          size_t fieldLen,
                                          const char ** pValueLoc,
                                          size_t * pValueLen );

/**
 * @brief The "on_header_field" callback for the HTTP parser used by the
 * #findHeaderInResponse function. The callback checks whether the parser
 * header field matched the header being searched for, and sets a flag to
 * represent reception of the header accordingly.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 * @param[in] pFieldLoc The location of the parsed header field in the response
 * buffer.
 * @param[in] fieldLen The length of the header field.
 *
 * @return Returns #HTTP_PARSER_CONTINUE_PARSING to indicate continuation with
 * parsing.
 */
static int findHeaderFieldParserCallback( http_parser * pHttpParser,
                                          const char * pFieldLoc,
                                          size_t fieldLen );

/**
 * @brief The "on_header_value" callback for the HTTP parser used by the
 * #findHeaderInResponse function. The callback sets the user-provided output
 * parameters for header value if the requested header's field was found in the
 * @ref findHeaderFieldParserCallback function.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 * @param[in] pValueLoc The location of the parsed header value in the response
 * buffer.
 * @param[in] valueLen The length of the header value.
 *
 * @return Returns #HTTP_PARSER_STOP_PARSING, if the header field/value pair are
 * found, otherwise #HTTP_PARSER_CONTINUE_PARSING is returned.
 */
static int findHeaderValueParserCallback( http_parser * pHttpParser,
                                          const char * pValueLoc,
                                          size_t valueLen );

/**
 * @brief The "on_header_complete" callback for the HTTP parser used by the
 * #findHeaderInResponse function.
 *
 * This callback will only be invoked if the requested header is not found in
 * the response. This callback is used to signal the parser to halt execution
 * if the requested header is not found.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 *
 * @return Returns #HTTP_PARSER_STOP_PARSING for the parser to halt further
 * execution, as all headers have been parsed in the response.
 */
static int findHeaderOnHeaderCompleteCallback( http_parser * pHttpParser );


/**
 * @brief Initialize the parsing context for parsing a response fresh from the
 * server.
 *
 * @param[in] pParsingContext The parsing context to initialize.
 * @param[in] pRequestHeaders Request headers for the corresponding HTTP request.
 */
static void initializeParsingContextForFirstResponse( HTTPParsingContext_t * pParsingContext,
                                                      const HTTPRequestHeaders_t * pRequestHeaders );

/**
 * @brief Parses the response buffer in @p pResponse.
 *
 * This function may be invoked multiple times for different parts of the the
 * HTTP response. The state of what was last parsed in the response is kept in
 * @p pParsingContext.
 *
 * @param[in,out] pParsingContext The response parsing state.
 * @param[in,out] pResponse The response information to be updated.
 * @param[in] parseLen The next length to parse in pResponse->pBuffer.
 *
 * @return One of the following:
 * - #HTTPSuccess
 * - #HTTPInvalidParameter
 * - Please see #processHttpParserError for parsing errors returned.
 */
static HTTPStatus_t parseHttpResponse( HTTPParsingContext_t * pParsingContext,
                                       HTTPResponse_t * pResponse,
                                       size_t parseLen );

/**
 * @brief Callback invoked during http_parser_execute() to indicate the start of
 * the HTTP response message.
 *
 * This callback is invoked when an "H" in the "HTTP/1.1" that starts a response
 * is found.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 *
 * @return #HTTP_PARSER_CONTINUE_PARSING to continue parsing.
 */
static int httpParserOnMessageBeginCallback( http_parser * pHttpParser );

/**
 * @brief Callback invoked during http_parser_execute() when the HTTP response
 * status-code and its associated reason-phrase are found.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 * @param[in] pLoc Location of the HTTP response reason-phrase string in the
 * response message buffer.
 * @param[in] length Length of the HTTP response status code string.
 *
 * @return #HTTP_PARSER_CONTINUE_PARSING to continue parsing.
 */
static int httpParserOnStatusCallback( http_parser * pHttpParser,
                                       const char * pLoc,
                                       size_t length );

/**
 * @brief Callback invoked during http_parser_execute() when an HTTP response
 * header field is found.
 *
 * If only part of the header field was found, then parsing of the next part of
 * the response message will invoke this callback in succession.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 * @param[in] pLoc Location of the header field string in the response
 * message buffer.
 * @param[in] length Length of the header field.
 *
 * @return #HTTP_PARSER_CONTINUE_PARSING to continue parsing.
 */
static int httpParserOnHeaderFieldCallback( http_parser * pHttpParser,
                                            const char * pLoc,
                                            size_t length );

/**
 * @brief Callback invoked during http_parser_execute() when an HTTP response
 * header value is found.
 *
 * This header value corresponds to the header field that was found in the
 * immediately preceding httpParserOnHeaderFieldCallback().
 *
 * If only part of the header value was found, then parsing of the next part of
 * the response message will invoke this callback in succession.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 * @param[in] pLoc Location of the header value in the response message buffer.
 * @param[in] length Length of the header value.
 *
 * @return #HTTP_PARSER_CONTINUE_PARSING to continue parsing.
 */
static int httpParserOnHeaderValueCallback( http_parser * pHttpParser,
                                            const char * pLoc,
                                            size_t length );

/**
 * @brief Callback invoked during http_parser_execute() when the end of the
 * headers are found.
 *
 * The end of the headers is signaled in a HTTP response message by another
 * "\r\n" after the final header line.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 *
 * @return #HTTP_PARSER_CONTINUE_PARSING to continue parsing.
 * #HTTP_PARSER_STOP_PARSING is returned if the response is for a HEAD request.
 */
static int httpParserOnHeadersCompleteCallback( http_parser * pHttpParser );

/**
 * @brief Callback invoked during http_parser_execute() when the HTTP response
 * body is found.
 *
 * If only part of the response body was found, then parsing of the next part of
 * the response message will invoke this callback in succession.
 *
 * This callback will be also invoked in succession if the response body is of
 * type "Transfer-Encoding: chunked". This callback will be invoked after each
 * chunk header.
 *
 * The follow is an example of a Transfer-Encoding chunked response:
 *
 * @code
 * HTTP/1.1 200 OK\r\n
 * Content-Type: text/plain\r\n
 * Transfer-Encoding: chunked\r\n
 * \r\n
 * d\r\n
 * Hello World! \r\n
 * 7\r\n
 * I am a \r\n
 * a\r\n
 * developer.\r\n
 * 0\r\n
 * \r\n
 * @endcode
 *
 * The first invocation of this callback will contain @p pLoc = "Hello World!"
 * and @p length = 13.
 * The second invocation of this callback will contain @p pLoc = "I am a " and
 * @p length = 7.
 * The third invocation of this callback will contain @p pLoc = "developer." and
 * @p length = 10.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 * @param[in] pLoc - Pointer to the body string in the response message buffer.
 * @param[in] length - The length of the body found.
 *
 * @return Zero to continue parsing. All other return values will stop parsing
 * and http_parser_execute() will return with status HPE_CB_body.
 */
static int httpParserOnBodyCallback( http_parser * pHttpParser,
                                     const char * pLoc,
                                     size_t length );

/**
 * @brief Callback invoked during http_parser_execute() to indicate the the
 * completion of an HTTP response message.
 *
 * When there is no response body, the end of the response message is when the
 * headers end. This is indicated by another "\r\n" after the final header line.
 *
 * When there is response body, the end of the response message is when the
 * full "Content-Length" value is parsed following the end of the headers. If
 * there is no Content-Length header, then http_parser_execute() expects a
 * zero length-ed parsing data to indicate the end of the response.
 *
 * For a "Transfer-Encoding: chunked" type of response message, the complete
 * response message is signaled by a terminating chunk header with length zero.
 *
 * See https://github.com/nodejs/http-parser for more information.
 *
 * @param[in] pHttpParser Parsing object containing state and callback context.
 *
 * @return Zero to continue parsing. All other return values will stop parsing
 * and http_parser_execute() will return with status HPE_CB_message_complete.
 */
static int httpParserOnMessageCompleteCallback( http_parser * pHttpParser );

/**
 * @brief When a complete header is found the HTTP response header count
 * increases and the application is notified.
 *
 * This function is invoked only in callbacks that could follow
 * #httpParserOnHeaderValueCallback. These callbacks are
 * #httpParserOnHeaderFieldCallback and #httpParserOnHeadersCompleteCallback.
 * A header field and value is not is not known to be complete until
 * #httpParserOnHeaderValueCallback is not called in succession.
 *
 * @param[in] pParsingContext Parsing state containing information to notify
 * the application of a complete header.
 */
static void processCompleteHeader( HTTPParsingContext_t * pParsingContext );

/**
 * @brief When parsing is complete an error could be indicated in
 * pHttpParser->http_errno. This function translates that error into a library
 * specific error code.
 *
 * @param[in] pHttpParser Third-party HTTP parsing context.
 *
 * @return One of the following:
 * - #HTTPSuccess
 * - #HTTPSecurityAlertResponseHeadersSizeLimitExceeded
 * - #HTTPSecurityAlertExtraneousResponseData
 * - #HTTPSecurityAlertInvalidChunkHeader
 * - #HTTPSecurityAlertInvalidProtocolVersion
 * - #HTTPSecurityAlertInvalidStatusCode
 * - #HTTPSecurityAlertInvalidCharacter
 * - #HTTPSecurityAlertInvalidContentLength
 * - #HTTPParserInternalError
 */
static HTTPStatus_t processHttpParserError( const http_parser * pHttpParser );

/*-----------------------------------------------------------*/

static uint32_t getZeroTimestampMs( void )
{
    return 0U;
}

/*-----------------------------------------------------------*/

static void processCompleteHeader( HTTPParsingContext_t * pParsingContext )
{
    HTTPResponse_t * pResponse = NULL;

    assert( pParsingContext != NULL );
    assert( pParsingContext->pResponse != NULL );

    pResponse = pParsingContext->pResponse;

    /* A header is complete when both the last header field and value have been
     * filled in. */
    if( ( pParsingContext->pLastHeaderField != NULL ) &&
        ( pParsingContext->pLastHeaderValue != NULL ) )
    {
        assert( pResponse->headerCount < SIZE_MAX );
        /* Increase the header count. */
        pResponse->headerCount++;

        LogDebug( ( "Response parsing: Found complete header: "
                    "HeaderField=%.*s, HeaderValue=%.*s",
                    ( int ) ( pParsingContext->lastHeaderFieldLen ),
                    pParsingContext->pLastHeaderField,
                    ( int ) ( pParsingContext->lastHeaderValueLen ),
                    pParsingContext->pLastHeaderValue ) );

        if (0 == strncasecmp(pParsingContext->pLastHeaderField, "Transfer-Encoding", pParsingContext->lastHeaderFieldLen) && 
            0 == strncasecmp(pParsingContext->pLastHeaderValue, "chunked", pParsingContext->lastHeaderValueLen)) {
                LogDebug( ( "Http check chuned"));
        }

        /* If the application registered a callback, then it must be notified. */
        if( pResponse->pHeaderParsingCallback != NULL )
        {
            pResponse->pHeaderParsingCallback->onHeaderCallback(
                pResponse->pHeaderParsingCallback->pContext,
                pParsingContext->pLastHeaderField,
                pParsingContext->lastHeaderFieldLen,
                pParsingContext->pLastHeaderValue,
                pParsingContext->lastHeaderValueLen,
                pResponse->statusCode );
        }

        /* Prepare the next header field and value for the first invocation of
         * httpParserOnHeaderFieldCallback() and
         * httpParserOnHeaderValueCallback(). */
        pParsingContext->pLastHeaderField = NULL;
        pParsingContext->lastHeaderFieldLen = 0U;
        pParsingContext->pLastHeaderValue = NULL;
        pParsingContext->lastHeaderValueLen = 0U;
    }
}

/*-----------------------------------------------------------*/

static int httpParserOnMessageBeginCallback( http_parser * pHttpParser )
{
    HTTPParsingContext_t * pParsingContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );

    /* Parsing has initiated. */
    pParsingContext->state = HTTP_PARSING_INCOMPLETE;

    LogDebug( ( "Response parsing: Found the start of the response message." ) );

    return HTTP_PARSER_CONTINUE_PARSING;
}

/*-----------------------------------------------------------*/
static int httpParserOnChunkHeaderCallback( http_parser * pHttpParser )
{
    HTTPParsingContext_t * pParsingContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );

    LogDebug( ( "http Parser On Chunk Header, Chunk Length %d" , pHttpParser->content_length));

    return HTTP_PARSER_CONTINUE_PARSING;
}
/*-----------------------------------------------------------*/

static int httpParserOnChunkComleteCallback( http_parser * pHttpParser )
{
    HTTPParsingContext_t * pParsingContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );

    LogDebug( ( "http Parser On Chunk Comlete" ) );

    return HTTP_PARSER_CONTINUE_PARSING;
}
/*-----------------------------------------------------------*/

static int httpParserOnStatusCallback( http_parser * pHttpParser,
                                       const char * pLoc,
                                       size_t length )
{
    HTTPParsingContext_t * pParsingContext = NULL;
    HTTPResponse_t * pResponse = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );
    assert( pLoc != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );
    pResponse = pParsingContext->pResponse;

    assert( pResponse != NULL );

    /* Set the location of what to parse next. */
    pParsingContext->pBufferCur = pLoc + length;

    /* Initialize the first header field and value to be passed to the user
     * callback. */
    pParsingContext->pLastHeaderField = NULL;
    pParsingContext->lastHeaderFieldLen = 0U;
    pParsingContext->pLastHeaderValue = NULL;
    pParsingContext->lastHeaderValueLen = 0U;

    /* httpParserOnStatusCallback() is reached because http_parser_execute() has
     * successfully read the HTTP response status code. */
    pResponse->statusCode = ( uint16_t ) ( pHttpParser->status_code );

    LogDebug( ( "Response parsing: Found the Reason-Phrase: "
                "StatusCode=%u, ReasonPhrase=%.*s",
                ( unsigned int ) pResponse->statusCode,
                ( int ) length,
                pLoc ) );

    return HTTP_PARSER_CONTINUE_PARSING;
}

/*-----------------------------------------------------------*/

static int httpParserOnHeaderFieldCallback( http_parser * pHttpParser,
                                            const char * pLoc,
                                            size_t length )
{
    HTTPParsingContext_t * pParsingContext = NULL;
    HTTPResponse_t * pResponse = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );
    assert( pLoc != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );
    pResponse = pParsingContext->pResponse;

    assert( pResponse != NULL );

    /* If this is the first time httpParserOnHeaderFieldCallback() has been
     * invoked on a response, then the start of the response headers is NULL. */
    if( pResponse->pHeaders == NULL )
    {
        pResponse->pHeaders = ( const uint8_t * ) pLoc;
    }

    /* Set the location of what to parse next. */
    pParsingContext->pBufferCur = pLoc + length;

    /* The httpParserOnHeaderFieldCallback() always follows the
     * httpParserOnHeaderValueCallback() if there is another header field. When
     * httpParserOnHeaderValueCallback() is not called in succession, then a
     * complete header has been found. */
    processCompleteHeader( pParsingContext );

    /* If httpParserOnHeaderFieldCallback() is invoked in succession, then the
     * last time http_parser_execute() was called only part of the header field
     * was parsed. The indication of successive invocations is a non-NULL
     * pParsingContext->pLastHeaderField. */
    if( pParsingContext->pLastHeaderField == NULL )
    {
        pParsingContext->pLastHeaderField = pLoc;
        pParsingContext->lastHeaderFieldLen = length;
    }
    else
    {
        assert( pParsingContext->lastHeaderFieldLen <= SIZE_MAX - length );
        pParsingContext->lastHeaderFieldLen += length;
    }

    LogDebug( ( "Response parsing: Found a header field: "
                "HeaderField=%.*s",
                ( int ) length,
                pLoc ) );

    return HTTP_PARSER_CONTINUE_PARSING;
}

/*-----------------------------------------------------------*/

static int httpParserOnHeaderValueCallback( http_parser * pHttpParser,
                                            const char * pLoc,
                                            size_t length )
{
    HTTPParsingContext_t * pParsingContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );
    assert( pLoc != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );

    /* Set the location of what to parse next. */
    pParsingContext->pBufferCur = pLoc + length;

    /* If httpParserOnHeaderValueCallback() is invoked in succession, then the
     * last time http_parser_execute() was called only part of the header field
     * was parsed. The indication of successive invocations is a non-NULL
     * pParsingContext->pLastHeaderField. */
    if( pParsingContext->pLastHeaderValue == NULL )
    {
        pParsingContext->pLastHeaderValue = pLoc;
        pParsingContext->lastHeaderValueLen = length;
    }
    else
    {
        pParsingContext->lastHeaderValueLen += length;
    }

    /* Given that httpParserOnHeaderFieldCallback() is ALWAYS invoked before
     * httpParserOnHeaderValueCallback() is invoked, then the last header field
     * should never be NULL. This would indicate a bug in the http-parser
     * library. */
    assert( pParsingContext->pLastHeaderField != NULL );

    LogDebug( ( "Response parsing: Found a header value: "
                "HeaderValue=%.*s",
                ( int ) length,
                pLoc ) );

    return HTTP_PARSER_CONTINUE_PARSING;
}

/*-----------------------------------------------------------*/

static int httpParserOnHeadersCompleteCallback( http_parser * pHttpParser )
{
    int shouldContinueParse = HTTP_PARSER_CONTINUE_PARSING;
    HTTPParsingContext_t * pParsingContext = NULL;
    HTTPResponse_t * pResponse = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );
    pResponse = pParsingContext->pResponse;

    assert( pResponse != NULL );
    assert( pParsingContext->pBufferCur != NULL );

    /* The current location to parse was updated in previous callbacks and MUST
     * always be within the response buffer. */
    assert( pParsingContext->pBufferCur >= ( const char * ) ( pResponse->pBuffer ) );
    assert( pParsingContext->pBufferCur < ( const char * ) ( pResponse->pBuffer + pResponse->bufferLen ) );

    /* `\r\n\r\n`, `\r\n\n`, `\n\r\n`, and `\n\n` are all valid indicators of
     * the end of the response headers. To reduce complexity these characters
     * are not included in the response headers length returned to the user. */

    /* If headers existed, then pResponse->pHeaders was set during the first
     * call to httpParserOnHeaderFieldCallback(). */
    if( pResponse->pHeaders != NULL )
    {
        /* The start of the headers ALWAYS come before the the end of the headers. */
        assert( ( const char * ) ( pResponse->pHeaders ) < pParsingContext->pBufferCur );

        /* MISRA Rule 10.8 flags the following line for casting from a signed
         * pointer difference to a size_t. This rule is suppressed because in
         * in the previous statement it is asserted that the pointer difference
         * will never be negative. */
        /* coverity[misra_c_2012_rule_10_8_violation] */
        pResponse->headersLen = ( size_t ) ( pParsingContext->pBufferCur - ( const char * ) ( pResponse->pHeaders ) );
    }
    else
    {
        pResponse->headersLen = 0U;
    }

    /* If the Content-Length header was found, then pHttpParser->content_length
     * will not be equal to the maximum 64 bit integer. */
    if( pHttpParser->content_length != UINT64_MAX )
    {
        pResponse->contentLength = ( size_t ) ( pHttpParser->content_length );
    }
    else
    {
        pResponse->contentLength = 0U;
    }

    /* If the Connection: close header was found this flag will be set. */
    if( ( pHttpParser->flags & ( unsigned int ) ( F_CONNECTION_CLOSE ) ) != 0U )
    {
        pResponse->respFlags |= HTTP_RESPONSE_CONNECTION_CLOSE_FLAG;
    }

    /* If the Connection: keep-alive header was found this flag will be set. */
    if( ( pHttpParser->flags & ( unsigned int ) ( F_CONNECTION_KEEP_ALIVE ) ) != 0U )
    {
        pResponse->respFlags |= HTTP_RESPONSE_CONNECTION_KEEP_ALIVE_FLAG;
    }

    /* http_parser_execute() requires that callback implementations must
     * indicate that parsing stops on headers complete, if response is to a HEAD
     * request. A HEAD response will contain Content-Length, but no body. If
     * the parser is not stopped here, then it will try to keep parsing past the
     * end of the headers up to the Content-Length found. */
    if( pParsingContext->isHeadResponse == 1U )
    {
        shouldContinueParse = HTTP_PARSER_STOP_PARSING;
    }

    /* If headers are present in the response, then
     * httpParserOnHeadersCompleteCallback() always follows
     * the httpParserOnHeaderValueCallback(). When
     * httpParserOnHeaderValueCallback() is not called in succession, then a
     * complete header has been found. */
    processCompleteHeader( pParsingContext );

    LogDebug( ( "Response parsing: Found the end of the headers." ) );

    return shouldContinueParse;
}

/*-----------------------------------------------------------*/

static int httpParserOnBodyCallback( http_parser * pHttpParser,
                                     const char * pLoc,
                                     size_t length )
{
    int shouldContinueParse = HTTP_PARSER_CONTINUE_PARSING;
    HTTPParsingContext_t * pParsingContext = NULL;
    HTTPResponse_t * pResponse = NULL;
    char * pNextWriteLoc = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );
    assert( pLoc != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );
    pResponse = pParsingContext->pResponse;

    assert( pResponse != NULL );
    assert( pResponse->pBuffer != NULL );

    if (pHttpParser->flags & F_CHUNKED) {
        if ((pResponse->bodyLen + length) > HTTP_MAX_RESPONSE_CHUNK_SIZE_BYTES) {
            shouldContinueParse = HTTP_PARSER_STOP_PARSING;
            LogError( ( "Chunk size is too large"
                        "See HTTP_MAX_RESPONSE_CHUNK_SIZE_BYTES=%lu",
                        ( unsigned long ) HTTP_MAX_RESPONSE_CHUNK_SIZE_BYTES ) );
            return HTTP_PARSER_STOP_PARSING;
        }
        memcpy( pResponse->pBody + pResponse->bodyLen, pLoc, length);
        pResponse->bodyLen += length;
        return HTTP_PARSER_CONTINUE_PARSING;
    }

    // assert( pLoc >= ( const char * ) ( pResponse->pBuffer ) );
    // assert( pLoc < ( const char * ) ( pResponse->pBuffer + pResponse->bufferLen ) );

    /* If this is the first time httpParserOnBodyCallback() has been invoked,
     * then the start of the response body is NULL. */
    if( pResponse->pBody == NULL )
    {
        /* Ideally the start of the body should follow right after the header
         * end indicating characters, but to reduce complexity and ensure users
         * are given the correct start of the body, we set the start of the body
         * to what the parser tells us is the start. This could come after the
         * initial transfer encoding chunked header. */
        pResponse->pBody = ( const uint8_t * ) ( pLoc );
        pResponse->bodyLen = 0U;
    }

    /* The next location to write. */

    /* MISRA Rule 11.8 flags casting away the const qualifier in the pointer
     * type. This rule is suppressed because when the body is of transfer
     * encoding chunked, the body must be copied over the chunk headers that
     * precede it. This is done to have a contiguous response body. This does
     * affect future parsing as the changed segment will always be before the
     * next place to parse. */
    /* coverity[misra_c_2012_rule_11_8_violation] */
    pNextWriteLoc = ( char * ) ( pResponse->pBody + pResponse->bodyLen );

    /* If the response is of type Transfer-Encoding: chunked, then actual body
     * will follow the the chunked header. This body data is in a later location
     * and must be moved up in the buffer. When pLoc is greater than the current
     * end of the body, that signals the parser found a chunk header. */

    /* MISRA Rule 18.3 flags pLoc and pNextWriteLoc as pointing to two different
     * objects. This rule is suppressed because both pNextWriteLoc and pLoc
     * point to a location in the response buffer. */
    /* coverity[pointer_parameter] */
    /* coverity[misra_c_2012_rule_18_3_violation] */
    if( pLoc > pNextWriteLoc )
    {
        /* memmove is used instead of memcpy because memcpy has undefined behavior
         * when source and destination locations in memory overlap. */
        ( void ) memmove( pNextWriteLoc, pLoc, length );
    }

    /* Increase the length of the body found. */
    pResponse->bodyLen += length;

    /* Set the next location of parsing. */
    pParsingContext->pBufferCur = pLoc + length;

    LogDebug( ( "Response parsing: Found the response body: "
                "BodyLength=%lu",
                ( unsigned long ) length ) );

    return shouldContinueParse;
}

/*-----------------------------------------------------------*/

static int httpParserOnMessageCompleteCallback( http_parser * pHttpParser )
{
    HTTPParsingContext_t * pParsingContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );

    pParsingContext = ( HTTPParsingContext_t * ) ( pHttpParser->data );

    /* The response message is complete. */
    pParsingContext->state = HTTP_PARSING_COMPLETE;
    pParsingContext->recvState = HTTP_RECV_DONE;

    LogDebug( ( "Response parsing: Response message complete." ) );

    return HTTP_PARSER_CONTINUE_PARSING;
}

/*-----------------------------------------------------------*/

static void initializeParsingContextForFirstResponse( HTTPParsingContext_t * pParsingContext,
                                                      const HTTPRequestHeaders_t * pRequestHeaders )
{
    assert( pParsingContext != NULL );
    assert( pRequestHeaders != NULL );
    assert( pRequestHeaders->headersLen >= HTTP_MINIMUM_REQUEST_LINE_LENGTH );

    /* Initialize the third-party HTTP parser to parse responses. */
    http_parser_init( &( pParsingContext->httpParser ), HTTP_RESPONSE );

    /* The parser will return an error if this header size limit is exceeded. */
    http_parser_set_max_header_size( HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES );

    /* No response has been parsed yet. */
    pParsingContext->state = HTTP_PARSING_NONE;

    /* No response to update is associated with this parsing context yet. */
    pParsingContext->pResponse = NULL;

    /* The parsing context needs to know if the expected response is to a HEAD
     * request. For a HEAD response, the third-party parser requires parsing is
     * indicated to stop by returning a 1 from httpParserOnHeadersCompleteCallback().
     * If this is not done, the parser will not indicate the message is complete. */
    if( strncmp( ( const char * ) ( pRequestHeaders->pBuffer ),
                 HTTP_METHOD_HEAD,
                 sizeof( HTTP_METHOD_HEAD ) - 1U ) == 0 )
    {
        pParsingContext->isHeadResponse = 1U;
    }
}

/*-----------------------------------------------------------*/

static HTTPStatus_t processHttpParserError( const http_parser * pHttpParser )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    assert( pHttpParser != NULL );

    switch( ( enum http_errno ) ( pHttpParser->http_errno ) )
    {
        case HPE_OK:
            /* There were no errors. */
            break;

        case HPE_INVALID_EOF_STATE:

            /* In this case the parser was passed a length of zero, which indicates
             * an EOF from the server, in the middle of parsing the response.
             * This case is already handled by checking HTTPParsingContext_t.state. */
            break;

        case HPE_HEADER_OVERFLOW:
            LogError( ( "Response parsing error: Header byte limit "
                        "exceeded: HeaderByteLimit=%u",
                        HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES ) );
            returnStatus = HTTPSecurityAlertResponseHeadersSizeLimitExceeded;
            break;

        case HPE_CLOSED_CONNECTION:
            LogError( ( "Response parsing error: Data received past complete "
                        "response with \"Connection: close\" header present." ) );
            returnStatus = HTTPSecurityAlertExtraneousResponseData;
            break;

        case HPE_INVALID_CHUNK_SIZE:

            /* http_parser_execute() does not give feedback on the exact failing
             * character and location. */
            LogError( ( "Response parsing error: Invalid character found in "
                        "chunk header." ) );
            returnStatus = HTTPSecurityAlertInvalidChunkHeader;
            break;

        case HPE_INVALID_VERSION:

            /* http_parser_execute() does not give feedback on the exact failing
             * character and location. */
            LogError( ( "Response parsing error: Invalid character found in "
                        "HTTP protocol version." ) );
            returnStatus = HTTPSecurityAlertInvalidProtocolVersion;
            break;

        case HPE_INVALID_STATUS:

            /* There could be an invalid character or the status code number
             * could be out of range. This feedback is not given back by the
             * http-parser library. */
            LogError( ( "Response parsing error: Invalid Status code." ) );
            returnStatus = HTTPSecurityAlertInvalidStatusCode;
            break;

        case HPE_STRICT:
        case HPE_INVALID_CONSTANT:
            LogError( ( "Response parsing error: Invalid character found in "
                        "Status-Line or header delimiters." ) );
            returnStatus = HTTPSecurityAlertInvalidCharacter;
            break;

        case HPE_LF_EXPECTED:
            LogError( ( "Response parsing error: Expected line-feed in header "
                        "not found." ) );
            returnStatus = HTTPSecurityAlertInvalidCharacter;
            break;

        case HPE_INVALID_HEADER_TOKEN:

            /* http_parser_execute() does not give feedback on the exact failing
             * character and location. */
            LogError( ( "Response parsing error: Invalid character found in "
                        "headers." ) );
            returnStatus = HTTPSecurityAlertInvalidCharacter;
            break;

        case HPE_INVALID_CONTENT_LENGTH:

            /* http_parser_execute() does not give feedback on the exact failing
             * character and location. */
            LogError( ( "Response parsing error: Invalid character found in "
                        "content-length headers." ) );
            returnStatus = HTTPSecurityAlertInvalidContentLength;
            break;

        case HPE_UNEXPECTED_CONTENT_LENGTH:
            LogError( ( "Response parsing error: A Content-Length header was "
                        "found when it shouldn't have been." ) );
            returnStatus = HTTPSecurityAlertInvalidContentLength;
            break;

        /* All other error cases cannot be triggered and indicate an error in the
         * third-party parsing library if found. */
        default:
            LogError( ( "Error in third-party http-parser library." ) );
            returnStatus = HTTPParserInternalError;
            break;
    }

    /* Errors with CB_ prepending are manual returns of non-zero in the
     * response parsing callback. */
    LogDebug( ( "http-parser errno description: %s",
                http_errno_description( HTTP_PARSER_ERRNO( pHttpParser ) ) ) );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t parseHttpResponse( HTTPParsingContext_t * pParsingContext,
                                       HTTPResponse_t * pResponse,
                                       size_t parseLen )
{
    HTTPStatus_t returnStatus;
    http_parser_settings parserSettings = { 0 };
    size_t bytesParsed = 0U;
    const char * parsingStartLoc = NULL;

    /* Disable unused variable warning. */
    ( void ) bytesParsed;

    assert( pParsingContext != NULL );
    assert( pResponse != NULL );

    /* If this is the first time this parsing context is used, then set the
     * response input. */
    if( pParsingContext->pResponse == NULL )
    {
        pParsingContext->pResponse = pResponse;
        pParsingContext->pBufferCur = ( const char * ) pResponse->pBuffer;

        /* Initialize the status-code returned in the response. */
        pResponse->statusCode = 0U;
        /* Initialize the start of the response body and length. */
        pResponse->pBody = NULL;
        pResponse->bodyLen = 0U;

        /* Initialize the start of the headers, its length, and the count for
         * the parsing that follows the status. */
        pResponse->pHeaders = NULL;
        pResponse->headersLen = 0U;
        pResponse->headerCount = 0U;
        /* Initialize the response flags. */
        pResponse->respFlags = 0U;
    }
    else
    {
        /* This function is currently private to the HTTP Client library. It is
         * therefore a development bug to have this function invoked in
         * succession without the same response. */
        assert( pParsingContext->pResponse == pResponse );
    }

    /* Initialize the callbacks that http_parser_execute will invoke. */
    http_parser_settings_init( &parserSettings );
    parserSettings.on_message_begin = httpParserOnMessageBeginCallback;
    parserSettings.on_status = httpParserOnStatusCallback;
    parserSettings.on_header_field = httpParserOnHeaderFieldCallback;
    parserSettings.on_header_value = httpParserOnHeaderValueCallback;
    parserSettings.on_headers_complete = httpParserOnHeadersCompleteCallback;
    parserSettings.on_body = httpParserOnBodyCallback;
    parserSettings.on_message_complete = httpParserOnMessageCompleteCallback;

    /* Setting this allows the parsing context and response to be carried to
     * each of the callbacks that http_parser_execute() will invoke. */
    pParsingContext->httpParser.data = pParsingContext;

    /* Save the starting response buffer location to parse. This is needed to
     * ensure that we move the next location to parse to exactly how many
     * characters were parsed in this call. */
    parsingStartLoc = pParsingContext->pBufferCur;

    /* This will begin the parsing. Each of the callbacks set in
     * parserSettings will be invoked as parts of the HTTP response are
     * reached. */
    bytesParsed = http_parser_execute( &( pParsingContext->httpParser ),
                                       &parserSettings,
                                       parsingStartLoc,
                                       parseLen );

    /* The next location to parse will always be after what has already
     * been parsed. */
    pParsingContext->pBufferCur = parsingStartLoc + bytesParsed;

    LogDebug( ( "Parsed HTTP Response buffer: BytesParsed=%lu, "
                "ExpectedBytesParsed=%lu",
                ( unsigned long ) bytesParsed,
                ( unsigned long ) parseLen ) );

    returnStatus = processHttpParserError( &( pParsingContext->httpParser ) );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static uint8_t convertInt32ToAscii( int32_t value,
                                    char * pBuffer,
                                    size_t bufferLength )
{
    /* As input value may be altered and MISRA C 2012 rule 17.8 prevents
     * modification of parameter, a local copy of the parameter is stored.
     * absoluteValue stores the positive version of the input value. Its type
     * remains the same type as the input value to avoid unnecessary casting on
     * a privately used variable. This variable's size will always be less
     * than INT32_MAX. */
    int32_t absoluteValue = value;
    uint8_t numOfDigits = 0U;
    uint8_t index = 0U;
    uint8_t isNegative = 0U;
    char temp = '\0';

    assert( pBuffer != NULL );
    assert( bufferLength >= MAX_INT32_NO_OF_DECIMAL_DIGITS );
    ( void ) bufferLength;

    /* If the value is negative, write the '-' (minus) character to the buffer. */
    if( value < 0 )
    {
        isNegative = 1U;

        *pBuffer = '-';

        /* Convert the value to its absolute representation. */
        absoluteValue = value * -1;
    }

    /* Write the absolute integer value in reverse ASCII representation. */
    do
    {
        pBuffer[ isNegative + numOfDigits ] = ( char ) ( ( absoluteValue % 10 ) + '0' );
        numOfDigits++;
        absoluteValue /= 10;
    } while( absoluteValue != 0 );

    /* Reverse the digits in the buffer to store the correct ASCII representation
     * of the value. */
    for( index = 0U; index < ( numOfDigits / 2U ); index++ )
    {
        temp = pBuffer[ isNegative + index ];
        pBuffer[ isNegative + index ] = pBuffer[ isNegative + numOfDigits - index - 1U ];
        pBuffer[ isNegative + numOfDigits - index - 1U ] = temp;
    }

    return( isNegative + numOfDigits );
}

/*-----------------------------------------------------------*/

static char * httpHeaderStrncpy( char * pDest,
                                 const char * pSrc,
                                 size_t len,
                                 uint8_t isField )
{
    size_t i = 0U;
    char * pRet = pDest;
    uint8_t hasError = 0U;

    assert( pDest != NULL );
    assert( pSrc != NULL );

    for( ; i < len; i++ )
    {
        if( pSrc[ i ] == CARRIAGE_RETURN_CHARACTER )
        {
            LogError( ( "Invalid character '\r' found in %.*s",
                        ( int ) len, pSrc ) );
            hasError = 1U;
        }
        else if( pSrc[ i ] == LINEFEED_CHARACTER )
        {
            LogError( ( "Invalid character '\n' found in %.*s",
                        ( int ) len, pSrc ) );
            hasError = 1U;
        }
        else if( ( isField == 1U ) && ( pSrc[ i ] == COLON_CHARACTER ) )
        {
            LogError( ( "Invalid character ':' found in %.*s",
                        ( int ) len, pSrc ) );
            hasError = 1U;
        }
        else
        {
            pDest[ i ] = pSrc[ i ];
        }

        if( hasError == 1U )
        {
            pRet = NULL;
            break;
        }
    }

    return pRet;
}

/*-----------------------------------------------------------*/

static int addSizeTNoOverflow( size_t a, size_t b, size_t * pResult )
{
    assert( pResult != NULL );

    if( a > ( SIZE_MAX - b ) )
    {
        return 0;
    }

    *pResult = a + b;
    return 1;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t addHeader( HTTPRequestHeaders_t * pRequestHeaders,
                               const char * pField,
                               size_t fieldLen,
                               const char * pValue,
                               size_t valueLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    char * pBufferCur = NULL;
    size_t toAddLen = 0U;
    size_t headerLineSepTotal = 0U;
    size_t totalRequiredLen = 0U;
    size_t backtrackHeaderLen = 0U;

    assert( pRequestHeaders != NULL );
    assert( pRequestHeaders->pBuffer != NULL );
    assert( pField != NULL );
    assert( pValue != NULL );
    assert( fieldLen != 0U );
    assert( valueLen != 0U );

    pBufferCur = ( char * ) ( pRequestHeaders->pBuffer + pRequestHeaders->headersLen );
    backtrackHeaderLen = pRequestHeaders->headersLen;

    /* Backtrack before trailing "\r\n" (HTTP header end) if it's already written.
     * Note that this method also writes trailing "\r\n" before returning.
     * The first condition prevents reading before start of the header. */
    if( ( HTTP_HEADER_END_INDICATOR_LEN <= pRequestHeaders->headersLen ) &&
        ( strncmp( ( char * ) pBufferCur - HTTP_HEADER_END_INDICATOR_LEN,
                   HTTP_HEADER_END_INDICATOR, HTTP_HEADER_END_INDICATOR_LEN ) == 0 ) )
    {
        backtrackHeaderLen -= HTTP_HEADER_LINE_SEPARATOR_LEN;
        pBufferCur -= HTTP_HEADER_LINE_SEPARATOR_LEN;
    }

    if( addSizeTNoOverflow( HTTP_HEADER_LINE_SEPARATOR_LEN,
                            HTTP_HEADER_LINE_SEPARATOR_LEN,
                            &headerLineSepTotal ) == 0 )
    {
        returnStatus = HTTPInsufficientMemory;
    }

    if( ( returnStatus == HTTPSuccess ) &&
        ( addSizeTNoOverflow( fieldLen, HTTP_HEADER_FIELD_SEPARATOR_LEN, &toAddLen ) == 0 ) )
    {
        returnStatus = HTTPInsufficientMemory;
    }

    if( ( returnStatus == HTTPSuccess ) &&
        ( addSizeTNoOverflow( toAddLen, valueLen, &toAddLen ) == 0 ) )
    {
        returnStatus = HTTPInsufficientMemory;
    }

    if( ( returnStatus == HTTPSuccess ) &&
        ( addSizeTNoOverflow( toAddLen, headerLineSepTotal, &toAddLen ) == 0 ) )
    {
        returnStatus = HTTPInsufficientMemory;
    }

    if( ( returnStatus == HTTPSuccess ) &&
        ( addSizeTNoOverflow( backtrackHeaderLen, toAddLen, &totalRequiredLen ) == 0 ) )
    {
        returnStatus = HTTPInsufficientMemory;
    }

    /* If we have enough room for the new header line, then write it to the
     * header buffer. */
    if( ( returnStatus == HTTPSuccess ) &&
        ( totalRequiredLen <= pRequestHeaders->bufferLen ) )
    {
        /* Write "<Field>: <Value> \r\n" to the headers buffer. */

        /* Copy the header name into the buffer. */
        if( httpHeaderStrncpy( pBufferCur, pField, fieldLen, HTTP_HEADER_STRNCPY_IS_FIELD ) == NULL )
        {
            returnStatus = HTTPSecurityAlertInvalidCharacter;
        }

        if( returnStatus == HTTPSuccess )
        {
            pBufferCur += fieldLen;

            /* Copy the field separator, ": ", into the buffer. */
            ( void ) memcpy( pBufferCur,
                              HTTP_HEADER_FIELD_SEPARATOR,
                              HTTP_HEADER_FIELD_SEPARATOR_LEN );

            pBufferCur += HTTP_HEADER_FIELD_SEPARATOR_LEN;

            /* Copy the header value into the buffer. */
            if( httpHeaderStrncpy( pBufferCur, pValue, valueLen, HTTP_HEADER_STRNCPY_IS_VALUE ) == NULL )
            {
                returnStatus = HTTPSecurityAlertInvalidCharacter;
            }
        }

        if( returnStatus == HTTPSuccess )
        {
            pBufferCur += valueLen;

            /* Copy the header end indicator, "\r\n\r\n" into the buffer. */
            ( void ) memcpy( pBufferCur,
                              HTTP_HEADER_END_INDICATOR,
                              HTTP_HEADER_END_INDICATOR_LEN );

            /* Update the headers length value only when everything is successful. */
            pRequestHeaders->headersLen = totalRequiredLen;
        }
    }
    else
    {
        LogError( ( "Unable to add header in buffer: "
                    "Buffer has insufficient memory: "
                    "RequiredBytes=%lu, RemainingBufferSize=%lu",
                    ( unsigned long ) toAddLen,
                    ( unsigned long ) ( pRequestHeaders->bufferLen - pRequestHeaders->headersLen ) ) );
        returnStatus = HTTPInsufficientMemory;
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t addRangeHeader( HTTPRequestHeaders_t * pRequestHeaders,
                                    int32_t rangeStartOrlastNbytes,
                                    int32_t rangeEnd )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    char rangeValueBuffer[ HTTP_MAX_RANGE_REQUEST_VALUE_LEN ];
    size_t rangeValueLength = 0U;

    assert( pRequestHeaders != NULL );

    /* This buffer uses a char type instead of the general purpose uint8_t
     * because the range value expected to be written is within the ASCII
     * character set. */
    ( void ) memset( rangeValueBuffer, 0, HTTP_MAX_RANGE_REQUEST_VALUE_LEN );

    /* Generate the value data for the Range Request header.*/

    /* Write the range value prefix in the buffer. */
    ( void ) memcpy( rangeValueBuffer,
                      HTTP_RANGE_REQUEST_HEADER_VALUE_PREFIX,
                      HTTP_RANGE_REQUEST_HEADER_VALUE_PREFIX_LEN );
    rangeValueLength += HTTP_RANGE_REQUEST_HEADER_VALUE_PREFIX_LEN;

    /* Write the range start value in the buffer. */
    rangeValueLength += convertInt32ToAscii( rangeStartOrlastNbytes,
                                             rangeValueBuffer + rangeValueLength,
                                             sizeof( rangeValueBuffer ) - rangeValueLength );

    /* Add remaining value data depending on the range specification type. */

    /* Add rangeEnd value if request is for [rangeStart, rangeEnd] byte range */
    if( rangeEnd != HTTP_RANGE_REQUEST_END_OF_FILE )
    {
        /* Write the "-" character to the buffer.*/
        *( rangeValueBuffer + rangeValueLength ) = DASH_CHARACTER;
        rangeValueLength += DASH_CHARACTER_LEN;

        /* Write the rangeEnd value of the request range to the buffer. */
        rangeValueLength += convertInt32ToAscii( rangeEnd,
                                                 rangeValueBuffer + rangeValueLength,
                                                 sizeof( rangeValueBuffer ) - rangeValueLength );
    }
    /* Case when request is for bytes in the range [rangeStart, EoF). */
    else if( rangeStartOrlastNbytes >= 0 )
    {
        /* Write the "-" character to the buffer.*/
        *( rangeValueBuffer + rangeValueLength ) = DASH_CHARACTER;
        rangeValueLength += DASH_CHARACTER_LEN;
    }
    else
    {
        /* Empty else MISRA 15.7 */
    }

    /* Add the Range Request header field and value to the buffer. */
    returnStatus = addHeader( pRequestHeaders,
                              HTTP_RANGE_REQUEST_HEADER_FIELD,
                              HTTP_RANGE_REQUEST_HEADER_FIELD_LEN,
                              rangeValueBuffer,
                              rangeValueLength );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t writeRequestLine( HTTPRequestHeaders_t * pRequestHeaders,
                                      const char * pMethod,
                                      size_t methodLen,
                                      const char * pPath,
                                      size_t pathLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    char * pBufferCur = NULL;
    size_t toAddLen = 0U;

    assert( pRequestHeaders != NULL );
    assert( pRequestHeaders->pBuffer != NULL );
    assert( pMethod != NULL );
    assert( methodLen != 0U );

    toAddLen = methodLen +                 \
               SPACE_CHARACTER_LEN +       \
               SPACE_CHARACTER_LEN +       \
               HTTP_PROTOCOL_VERSION_LEN + \
               HTTP_HEADER_LINE_SEPARATOR_LEN;

    pBufferCur = ( char * ) ( pRequestHeaders->pBuffer );
    toAddLen += ( ( pPath == NULL ) || ( pathLen == 0U ) ) ? HTTP_EMPTY_PATH_LEN : pathLen;

    if( ( toAddLen + pRequestHeaders->headersLen ) > pRequestHeaders->bufferLen )
    {
        returnStatus = HTTPInsufficientMemory;
    }

    if( returnStatus == HTTPSuccess )
    {
        /* Write "<METHOD> <PATH> HTTP/1.1\r\n" to start the HTTP header. */
        ( void ) strncpy( pBufferCur, pMethod, methodLen );
        pBufferCur += methodLen;

        *pBufferCur = SPACE_CHARACTER;
        pBufferCur += SPACE_CHARACTER_LEN;

        /* Use "/" as default value if <PATH> is NULL. */
        if( ( pPath == NULL ) || ( pathLen == 0U ) )
        {
            ( void ) memcpy( pBufferCur,
                              HTTP_EMPTY_PATH,
                              HTTP_EMPTY_PATH_LEN );
            pBufferCur += HTTP_EMPTY_PATH_LEN;
        }
        else
        {
            ( void ) strncpy( pBufferCur, pPath, pathLen );
            pBufferCur += pathLen;
        }

        *pBufferCur = SPACE_CHARACTER;
        pBufferCur += SPACE_CHARACTER_LEN;

        ( void ) memcpy( pBufferCur,
                          HTTP_PROTOCOL_VERSION,
                          HTTP_PROTOCOL_VERSION_LEN );
        pBufferCur += HTTP_PROTOCOL_VERSION_LEN;

        ( void ) memcpy( pBufferCur,
                          HTTP_HEADER_LINE_SEPARATOR,
                          HTTP_HEADER_LINE_SEPARATOR_LEN );
        pRequestHeaders->headersLen = toAddLen;
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

HTTPStatus_t HTTPClient_InitializeRequestHeaders( HTTPRequestHeaders_t * pRequestHeaders,
                                                  const HTTPRequestInfo_t * pRequestInfo )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    /* Check for NULL parameters. */
    if( pRequestHeaders == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestHeaders->pBuffer == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->pBuffer is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestInfo == NULL )
    {
        LogError( ( "Parameter check failed: pRequestInfo is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestInfo->pMethod == NULL )
    {
        LogError( ( "Parameter check failed: pRequestInfo->pMethod is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestInfo->pHost == NULL )
    {
        LogError( ( "Parameter check failed: pRequestInfo->pHost is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestInfo->methodLen == 0U )
    {
        LogError( ( "Parameter check failed: pRequestInfo->methodLen must be greater than 0." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestInfo->hostLen == 0U )
    {
        LogError( ( "Parameter check failed: pRequestInfo->hostLen must be greater than 0." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else
    {
        /* Empty else MISRA 15.7 */
    }

    if( returnStatus == HTTPSuccess )
    {
        /* Reset application-provided parameters. */
        pRequestHeaders->headersLen = 0U;

        /* Write "<METHOD> <PATH> HTTP/1.1\r\n" to start the HTTP header. */
        returnStatus = writeRequestLine( pRequestHeaders,
                                         pRequestInfo->pMethod,
                                         pRequestInfo->methodLen,
                                         pRequestInfo->pPath,
                                         pRequestInfo->pathLen );
    }

    if( returnStatus == HTTPSuccess )
    {
        /* Write "User-Agent: <Value>". */
        returnStatus = addHeader( pRequestHeaders,
                                  HTTP_USER_AGENT_FIELD,
                                  HTTP_USER_AGENT_FIELD_LEN,
                                  HTTP_USER_AGENT_VALUE,
                                  HTTP_USER_AGENT_VALUE_LEN );
    }

    if( returnStatus == HTTPSuccess )
    {
        /* Write "Host: <Value>". */
        returnStatus = addHeader( pRequestHeaders,
                                  HTTP_HOST_FIELD,
                                  HTTP_HOST_FIELD_LEN,
                                  pRequestInfo->pHost,
                                  pRequestInfo->hostLen );
    }

    if( returnStatus == HTTPSuccess )
    {
        if( ( HTTP_REQUEST_KEEP_ALIVE_FLAG & pRequestInfo->reqFlags ) != 0U )
        {
            /* Write "Connection: keep-alive". */
            returnStatus = addHeader( pRequestHeaders,
                                      HTTP_CONNECTION_FIELD,
                                      HTTP_CONNECTION_FIELD_LEN,
                                      HTTP_CONNECTION_KEEP_ALIVE_VALUE,
                                      HTTP_CONNECTION_KEEP_ALIVE_VALUE_LEN );
        }
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

HTTPStatus_t HTTPClient_AddHeader( HTTPRequestHeaders_t * pRequestHeaders,
                                   const char * pField,
                                   size_t fieldLen,
                                   const char * pValue,
                                   size_t valueLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    /* Check for NULL parameters. */
    if( pRequestHeaders == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestHeaders->pBuffer == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->pBuffer is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pField == NULL )
    {
        LogError( ( "Parameter check failed: pField is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pValue == NULL )
    {
        LogError( ( "Parameter check failed: pValue is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( fieldLen == 0U )
    {
        LogError( ( "Parameter check failed: fieldLen must be greater than 0." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( valueLen == 0U )
    {
        LogError( ( "Parameter check failed: valueLen must be greater than 0." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestHeaders->headersLen > pRequestHeaders->bufferLen )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->headersLen > pRequestHeaders->bufferLen." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else
    {
        /* Empty else MISRA 15.7 */
    }

    if( returnStatus == HTTPSuccess )
    {
        returnStatus = addHeader( pRequestHeaders,
                                  pField, fieldLen, pValue, valueLen );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

HTTPStatus_t HTTPClient_AddRangeHeader( HTTPRequestHeaders_t * pRequestHeaders,
                                        int32_t rangeStartOrlastNbytes,
                                        int32_t rangeEnd )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    if( pRequestHeaders == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestHeaders->pBuffer == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->pBuffer is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pRequestHeaders->headersLen > pRequestHeaders->bufferLen )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->headersLen > pRequestHeaders->bufferLen." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( rangeEnd < HTTP_RANGE_REQUEST_END_OF_FILE )
    {
        LogError( ( "Parameter check failed: rangeEnd is invalid: "
                    "rangeEnd should be >=-1: RangeEnd=%ld", ( long int ) rangeEnd ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( ( rangeStartOrlastNbytes < 0 ) &&
             ( rangeEnd != HTTP_RANGE_REQUEST_END_OF_FILE ) )
    {
        LogError( ( "Parameter check failed: Invalid range values: "
                    "rangeEnd should be -1 when rangeStart < 0: "
                    "RangeStart=%ld, RangeEnd=%ld",
                    ( long int ) rangeStartOrlastNbytes, ( long int ) rangeEnd ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( ( rangeEnd != HTTP_RANGE_REQUEST_END_OF_FILE ) &&
             ( rangeStartOrlastNbytes > rangeEnd ) )
    {
        LogError( ( "Parameter check failed: Invalid range values: "
                    "rangeStart should be < rangeEnd when both are >= 0: "
                    "RangeStart=%ld, RangeEnd=%ld",
                    ( long int ) rangeStartOrlastNbytes, ( long int ) rangeEnd ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( rangeStartOrlastNbytes == INT32_MIN )
    {
        LogError( ( "Parameter check failed: Arithmetic overflow detected: "
                    "rangeStart should be > -2147483648 (INT32_MIN): "
                    "RangeStart=%ld",
                    ( long int ) rangeStartOrlastNbytes ) );
        returnStatus = HTTPInvalidParameter;
    }
    else
    {
        returnStatus = addRangeHeader( pRequestHeaders,
                                       rangeStartOrlastNbytes,
                                       rangeEnd );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t sendHttpData( const TransportInterface_t * pTransport,
                                  HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                  const uint8_t * pData,
                                  size_t dataLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    const uint8_t * pIndex = pData;
    int32_t bytesSent = 0;
    size_t bytesRemaining = dataLen;
    uint32_t lastSendTimeMs = 0U, timeSinceLastSendMs = 0U;
    uint32_t retryTimeoutMs = HTTP_SEND_RETRY_TIMEOUT_MS;

    assert( pTransport != NULL );
    assert( pTransport->send != NULL );
    assert( pData != NULL );

    /* If the timestamp function was undefined by the application, then do not
     * retry the transport send. */
    if( getTimestampMs == getZeroTimestampMs )
    {
        retryTimeoutMs = 0U;
    }

    /* Initialize the last send time to allow retries, if 0 bytes are sent on
     * the first try. */
    lastSendTimeMs = getTimestampMs();

    /* Loop until all data is sent. */
    while( ( bytesRemaining > 0UL ) && ( returnStatus != HTTPNetworkError ) )
    {
        bytesSent = pTransport->send( pTransport->pNetworkContext,
                                      pIndex,
                                      bytesRemaining );

        /* BytesSent less than zero is an error. */
        if( bytesSent < 0 )
        {
            LogError( ( "Failed to send data: Transport send error: "
                        "TransportStatus=%ld", ( long int ) bytesSent ) );
            returnStatus = HTTPNetworkError;
        }
        else if( bytesSent > 0 )
        {
            /* It is a bug in the application's transport send implementation if
             * more bytes than expected are sent. To avoid a possible overflow
             * in converting bytesRemaining from unsigned to signed, this assert
             * must exist after the check for bytesSent being negative. */
            assert( ( size_t ) bytesSent <= bytesRemaining );

            /* Record the most recent time of successful transmission. */
            lastSendTimeMs = getTimestampMs();

            bytesRemaining -= ( size_t ) bytesSent;
            pIndex += bytesSent;
            LogDebug( ( "Sent data over the transport: "
                        "BytesSent=%ld, BytesRemaining=%lu, TotalBytesSent=%lu",
                        ( long int ) bytesSent,
                        ( unsigned long ) bytesRemaining,
                        ( unsigned long ) ( dataLen - bytesRemaining ) ) );
        }
        else
        {
            /* No bytes were sent over the network. */
            timeSinceLastSendMs = getTimestampMs() - lastSendTimeMs;

            /* Check for timeout if we have been waiting to send any data over
             * the network. */
            if( timeSinceLastSendMs >= retryTimeoutMs )
            {
                LogError( ( "Unable to send packet: Timed out in transport send." ) );
                returnStatus = HTTPNetworkError;
            }
        }
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t addContentLengthHeader( HTTPRequestHeaders_t * pRequestHeaders,
                                            size_t contentLength )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    char pContentLengthValue[ MAX_INT32_NO_OF_DECIMAL_DIGITS ] = { '\0' };
    uint8_t contentLengthValueNumBytes = 0U;

    assert( pRequestHeaders != NULL );
    assert( contentLength > 0U );

    contentLengthValueNumBytes = convertInt32ToAscii( ( int32_t ) contentLength,
                                                      pContentLengthValue,
                                                      sizeof( pContentLengthValue ) );

    returnStatus = addHeader( pRequestHeaders,
                              HTTP_CONTENT_LENGTH_FIELD,
                              HTTP_CONTENT_LENGTH_FIELD_LEN,
                              pContentLengthValue,
                              contentLengthValueNumBytes );

    if( returnStatus != HTTPSuccess )
    {
        LogError( ( "Failed to write Content-Length header to the request "
                    "header buffer: ContentLengthValue: %lu",
                    ( unsigned long ) contentLength ) );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t sendHttpHeaders( const TransportInterface_t * pTransport,
                                     HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                     HTTPRequestHeaders_t * pRequestHeaders,
                                     size_t reqBodyLen,
                                     uint32_t sendFlags )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    uint8_t shouldSendContentLength = 0U;

    assert( pTransport != NULL );
    assert( pTransport->send != NULL );
    assert( pRequestHeaders != NULL );

    /* Send the content length header if the flag to disable is not set and the
     * body length is greater than zero. */
    shouldSendContentLength = ( ( ( sendFlags & HTTP_SEND_DISABLE_CONTENT_LENGTH_FLAG ) == 0U ) &&
                                ( reqBodyLen > 0U ) ) ? 1U : 0U;

    if( shouldSendContentLength == 1U )
    {
        returnStatus = addContentLengthHeader( pRequestHeaders, reqBodyLen );
    }

    if( returnStatus == HTTPSuccess )
    {
        LogDebug( ( "Sending HTTP request headers: HeaderBytes=%lu",
                    ( unsigned long ) ( pRequestHeaders->headersLen ) ) );
        returnStatus = sendHttpData( pTransport,
                                     getTimestampMs,
                                     pRequestHeaders->pBuffer,
                                     pRequestHeaders->headersLen );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t sendHttpBody( const TransportInterface_t * pTransport,
                                  HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                  const uint8_t * pRequestBodyBuf,
                                  size_t reqBodyBufLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    assert( pTransport != NULL );
    assert( pTransport->send != NULL );
    assert( pRequestBodyBuf != NULL );

    /* Send the request body. */
    LogDebug( ( "Sending the HTTP request body: BodyBytes=%lu",
                ( unsigned long ) reqBodyBufLen ) );
    returnStatus = sendHttpData( pTransport, getTimestampMs, pRequestBodyBuf, reqBodyBufLen );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t getFinalResponseStatus( HTTPParsingState_t parsingState,
                                            size_t totalReceived,
                                            size_t responseBufferLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    assert( parsingState >= HTTP_PARSING_NONE &&
            parsingState <= HTTP_PARSING_COMPLETE );
    assert( totalReceived <= responseBufferLen );

    /* If no parsing occurred, that means network data was never received. */
    if( parsingState == HTTP_PARSING_NONE )
    {
        LogError( ( "Response not received: Zero returned from "
                    "transport recv: totalReceived=%lu",
                    ( unsigned long ) totalReceived ) );
        returnStatus = HTTPNoResponse;
    }
    else if( parsingState == HTTP_PARSING_INCOMPLETE )
    {
        if( totalReceived == responseBufferLen )
        {
            LogError( ( "Cannot receive complete response from transport"
                        " interface: Response buffer has insufficient "
                        "space: responseBufferLen=%lu",
                        ( unsigned long ) responseBufferLen ) );
            returnStatus = HTTPInsufficientMemory;
        }
        else
        {
            LogError( ( "Received partial response from transport "
                        "receive(): ResponseSize=%lu, TotalBufferSize=%lu",
                        ( unsigned long ) totalReceived,
                        ( unsigned long ) ( responseBufferLen - totalReceived ) ) );
            returnStatus = HTTPPartialResponse;
        }
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t receiveAndParseHttpResponse( const TransportInterface_t * pTransport,
                                                 HTTPResponse_t * pResponse,
                                                 const HTTPRequestHeaders_t * pRequestHeaders )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    size_t totalReceived = 0U;
    int32_t currentReceived = 0;
    HTTPParsingContext_t parsingContext = { 0 };
    uint8_t shouldRecv = 1U, shouldParse = 1U, timeoutReached = 0U;
    uint32_t lastRecvTimeMs = 0U, timeSinceLastRecvMs = 0U;
    uint32_t retryTimeoutMs = HTTP_RECV_RETRY_TIMEOUT_MS;

    assert( pTransport != NULL );
    assert( pTransport->recv != NULL );
    assert( pResponse != NULL );
    assert( pRequestHeaders != NULL );

    /* Initialize the parsing context for parsing the response received from the
     * network. */
    initializeParsingContextForFirstResponse( &parsingContext, pRequestHeaders );

    /* If the timestamp function was undefined by the application, then do not
     * retry the transport receive. */
    if( pResponse->getTime == getZeroTimestampMs )
    {
        retryTimeoutMs = 0U;
    }

    /* Initialize the last send time to allow retries, if 0 bytes are sent on
     * the first try. */
    lastRecvTimeMs = pResponse->getTime();

    while( shouldRecv == 1U )
    {
        /* Receive the HTTP response data into the pResponse->pBuffer. */
        currentReceived = pTransport->recv( pTransport->pNetworkContext,
                                            pResponse->pBuffer + totalReceived,
                                            pResponse->bufferLen - totalReceived );

        /* Transport receive errors are negative. */
        if( currentReceived < 0 )
        {
            LogError( ( "Failed to receive HTTP data: Transport recv() "
                        "returned error: TransportStatus=%ld",
                        ( long int ) currentReceived ) );
            returnStatus = HTTPNetworkError;

            /* Do not invoke the parser on network errors. */
            shouldParse = 0U;
        }
        else if( currentReceived > 0 )
        {
            /* Reset the time of the last data received when data is received. */
            lastRecvTimeMs = pResponse->getTime();

            /* Parsing is done on data as soon as it is received from the network.
             * Because we cannot know how large the HTTP response will be in
             * total, parsing will tell us if the end of the message is reached.*/
            shouldParse = 1U;
            totalReceived += currentReceived;
        }
        else
        {
            timeSinceLastRecvMs = pResponse->getTime() - lastRecvTimeMs;
            /* Do not invoke the response parsing for intermediate zero data. */
            shouldParse = 0U;

            /* Check if the allowed elapsed time between non-zero data has been
             * reached. */
            if( timeSinceLastRecvMs >= retryTimeoutMs )
            {
                /* Invoke the parsing upon this final zero data to indicate
                 * to the parser that there is no more data available from the
                 * server. */
                shouldParse = 1U;
                timeoutReached = 1U;
            }
        }

        if( shouldParse == 1U )
        {
            /* Data is received into the buffer is immediately parsed. Parsing
             * is invoked even with a length of zero. A length of zero indicates
             * to the parser that there is no more data from the server (EOF). */
            returnStatus = parseHttpResponse( &parsingContext,
                                              pResponse,
                                              currentReceived );
        }

        /* Reading should continue if there are no errors in the transport receive
         * or parsing, the retry on zero data timeout has not been reached, the
         * parser indicated the response message is not finished, and there is
         * room in the response buffer. */
        shouldRecv = ( ( returnStatus == HTTPSuccess ) &&
                       ( timeoutReached == 0U ) &&
                       ( parsingContext.state != HTTP_PARSING_COMPLETE ) &&
                       ( totalReceived < pResponse->bufferLen ) ) ? 1U : 0U;
    }

    if( returnStatus == HTTPSuccess )
    {
        /* If there are errors in receiving from the network or during parsing,
         * the final status of the response message is derived from the state of
         * the parsing and how much data is in the buffer. */
        returnStatus = getFinalResponseStatus( parsingContext.state,
                                               totalReceived,
                                               pResponse->bufferLen );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t sendHttpRequest( const TransportInterface_t * pTransport,
                                     HTTPClient_GetCurrentTimeFunc_t getTimestampMs,
                                     HTTPRequestHeaders_t * pRequestHeaders,
                                     const uint8_t * pRequestBodyBuf,
                                     size_t reqBodyBufLen,
                                     uint32_t sendFlags )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    assert( pTransport != NULL );
    assert( pRequestHeaders != NULL );
    assert( ( pRequestBodyBuf != NULL ) ||
            ( ( pRequestBodyBuf == NULL ) && ( reqBodyBufLen == 0 ) ) );
    assert( getTimestampMs != NULL );

    /* Send the headers, which are at one location in memory. */
    returnStatus = sendHttpHeaders( pTransport,
                                    getTimestampMs,
                                    pRequestHeaders,
                                    reqBodyBufLen,
                                    sendFlags );

    /* Send the body, which is at another location in memory. */
    if( returnStatus == HTTPSuccess )
    {
        if( pRequestBodyBuf != NULL )
        {
            returnStatus = sendHttpBody( pTransport,
                                         getTimestampMs,
                                         pRequestBodyBuf,
                                         reqBodyBufLen );
        }
        else
        {
            LogDebug( ( "A request body was not sent: pRequestBodyBuf is NULL." ) );
        }
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

HTTPStatus_t HTTPClient_Send( const TransportInterface_t * pTransport,
                              HTTPRequestHeaders_t * pRequestHeaders,
                              const uint8_t * pRequestBodyBuf,
                              size_t reqBodyBufLen,
                              HTTPResponse_t * pResponse,
                              uint32_t sendFlags )
{
    HTTPStatus_t returnStatus = HTTPInvalidParameter;

    if( pTransport == NULL )
    {
        LogError( ( "Parameter check failed: pTransport interface is NULL." ) );
    }
    else if( pTransport->send == NULL )
    {
        LogError( ( "Parameter check failed: pTransport->send is NULL." ) );
    }
    else if( pTransport->recv == NULL )
    {
        LogError( ( "Parameter check failed: pTransport->recv is NULL." ) );
    }
    else if( pRequestHeaders == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders is NULL." ) );
    }
    else if( pRequestHeaders->pBuffer == NULL )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->pBuffer is NULL." ) );
    }
    else if( pRequestHeaders->headersLen < HTTP_MINIMUM_REQUEST_LINE_LENGTH )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->headersLen "
                    "does not meet minimum the required length. "
                    "MinimumRequiredLength=%u, HeadersLength=%lu",
                    HTTP_MINIMUM_REQUEST_LINE_LENGTH,
                    ( unsigned long ) ( pRequestHeaders->headersLen ) ) );
    }
    else if( pRequestHeaders->headersLen > pRequestHeaders->bufferLen )
    {
        LogError( ( "Parameter check failed: pRequestHeaders->headersLen > "
                    "pRequestHeaders->bufferLen." ) );
    }
    else if( pResponse == NULL )
    {
        LogError( ( "Parameter check failed: pResponse is NULL. " ) );
    }
    else if( pResponse->pBuffer == NULL )
    {
        LogError( ( "Parameter check failed: pResponse->pBuffer is NULL." ) );
    }
    else if( ( pRequestBodyBuf == NULL ) && ( reqBodyBufLen > 0U ) )
    {
        /* If there is no body to send we must ensure that the reqBodyBufLen is
         * zero so that no Content-Length header is automatically written. */
        LogError( ( "Parameter check failed: pRequestBodyBuf is NULL, but "
                    "reqBodyBufLen is greater than zero." ) );
    }
    else if( reqBodyBufLen > ( size_t ) ( INT32_MAX ) )
    {
        /* This check is needed because convertInt32ToAscii() is used on the
         * reqBodyBufLen to create a Content-Length header value string. */
        LogError( ( "Parameter check failed: reqBodyBufLen > INT32_MAX."
                    "reqBodyBufLen=%lu",
                    ( unsigned long ) reqBodyBufLen ) );
    }
    else
    {
        if( pResponse->getTime == NULL )
        {
            /* Set a zero timestamp function when the application did not configure
             * one. */
            pResponse->getTime = getZeroTimestampMs;
        }

        returnStatus = HTTPSuccess;
    }

    if( returnStatus == HTTPSuccess )
    {
        returnStatus = sendHttpRequest( pTransport,
                                        pResponse->getTime,
                                        pRequestHeaders,
                                        pRequestBodyBuf,
                                        reqBodyBufLen,
                                        sendFlags );
    }

    if( returnStatus == HTTPSuccess )
    {
        returnStatus = receiveAndParseHttpResponse( pTransport,
                                                    pResponse,
                                                    pRequestHeaders );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static int findHeaderFieldParserCallback( http_parser * pHttpParser,
                                          const char * pFieldLoc,
                                          size_t fieldLen )
{
    findHeaderContext_t * pContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );
    assert( pFieldLoc != NULL );
    assert( fieldLen > 0U );

    pContext = ( findHeaderContext_t * ) pHttpParser->data;

    assert( pContext->pField != NULL );
    assert( pContext->fieldLen > 0U );

    /* The header found flags should not be set. */
    assert( pContext->fieldFound == 0U );
    assert( pContext->valueFound == 0U );

    /* Check whether the parsed header matches the header we are looking for. */
    /* Each header field consists of a case-insensitive field name (RFC 7230, section 3.2). */
    if( ( fieldLen == pContext->fieldLen ) &&
        ( strncasecmp( pContext->pField, pFieldLoc, fieldLen ) == 0 ) )
    {
        LogDebug( ( "Found header field in response: "
                    "HeaderName=%.*s, HeaderLocation=0x%p",
                    ( int ) fieldLen, pContext->pField, pFieldLoc ) );

        /* Set the flag to indicate that header has been found in response. */
        pContext->fieldFound = 1U;
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    return HTTP_PARSER_CONTINUE_PARSING;
}

/*-----------------------------------------------------------*/

static int findHeaderValueParserCallback( http_parser * pHttpParser,
                                          const char * pValueLoc,
                                          size_t valueLen )
{
    int retCode = HTTP_PARSER_CONTINUE_PARSING;
    findHeaderContext_t * pContext = NULL;

    assert( pHttpParser != NULL );
    assert( pHttpParser->data != NULL );
    assert( pValueLoc != NULL );

    pContext = ( findHeaderContext_t * ) pHttpParser->data;

    assert( pContext->pField != NULL );
    assert( pContext->fieldLen > 0U );
    assert( pContext->pValueLoc != NULL );
    assert( pContext->pValueLen != NULL );

    /* The header value found flag should not be set. */
    assert( pContext->valueFound == 0U );

    if( pContext->fieldFound == 1U )
    {
        LogDebug( ( "Found header value in response: "
                    "RequestedField=%.*s, ValueLocation=0x%p",
                    ( int ) ( pContext->fieldLen ), pContext->pField, pValueLoc ) );

        if( valueLen > 0U )
        {
            /* Populate the output parameters with the location of the header
             * value in the response buffer. */
            *pContext->pValueLoc = pValueLoc;
            *pContext->pValueLen = valueLen;
        }
        else
        {
            /* It is not invalid according to RFC 2616 to have an empty header
             * value. */
            *pContext->pValueLoc = NULL;
            *pContext->pValueLen = 0U;
        }

        /* Set the header value found flag. */
        pContext->valueFound = 1U;

        /* As we have found the value associated with the header, we don't need
         * to parse the response any further. */
        retCode = HTTP_PARSER_STOP_PARSING;
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    return retCode;
}

/*-----------------------------------------------------------*/

static int findHeaderOnHeaderCompleteCallback( http_parser * pHttpParser )
{
    findHeaderContext_t * pContext = NULL;

    /* Disable unused parameter warning. */
    ( void ) pHttpParser;
    /* Disable unused variable warning. */
    ( void ) pContext;

    assert( pHttpParser != NULL );

    pContext = ( findHeaderContext_t * ) pHttpParser->data;

    /* If we have reached here, all headers in the response have been parsed but the requested
     * header has not been found in the response buffer. */
    LogDebug( ( "Reached end of header parsing: Header not found in response: "
                "RequestedHeader=%.*s",
                ( int ) ( pContext->fieldLen ),
                pContext->pField ) );

    /* No further parsing is required; thus, indicate the parser to stop parsing. */
    return HTTP_PARSER_STOP_PARSING;
}

/*-----------------------------------------------------------*/

static HTTPStatus_t findHeaderInResponse( const uint8_t * pBuffer,
                                          size_t bufferLen,
                                          const char * pField,
                                          size_t fieldLen,
                                          const char ** pValueLoc,
                                          size_t * pValueLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;
    http_parser parser = { 0 };
    http_parser_settings parserSettings = { 0 };
    findHeaderContext_t context = { 0 };
    size_t numOfBytesParsed = 0U;

    context.pField = pField;
    context.fieldLen = fieldLen;
    context.pValueLoc = pValueLoc;
    context.pValueLen = pValueLen;
    context.fieldFound = 0U;
    context.valueFound = 0U;

    /* Disable unused variable warning. This variable is used only in logging. */
    ( void ) numOfBytesParsed;

    http_parser_init( &parser, HTTP_RESPONSE );

    /* Set the context for the parser. */
    parser.data = &context;

    /* The intention here to define callbacks just for searching the headers. We will
     * need to create a private context in httpParser->data that has the field and
     * value to update and pass back. */
    http_parser_settings_init( &parserSettings );
    parserSettings.on_header_field = findHeaderFieldParserCallback;
    parserSettings.on_header_value = findHeaderValueParserCallback;
    parserSettings.on_headers_complete = findHeaderOnHeaderCompleteCallback;

    /* Start parsing for the header! */
    numOfBytesParsed = http_parser_execute( &parser,
                                            &parserSettings,
                                            ( const char * ) pBuffer,
                                            bufferLen );

    LogDebug( ( "Parsed response for header search: NumBytesParsed=%lu",
                ( unsigned long ) numOfBytesParsed ) );

    if( context.fieldFound == 0U )
    {
        /* If header field is not found, then both the flags should be zero. */
        assert( context.valueFound == 0U );

        /* Header is not present in buffer. */
        LogWarn( ( "Header not found in response buffer: RequestedHeader=%.*s",
                   ( int ) fieldLen,
                   pField ) );

        returnStatus = HTTPHeaderNotFound;
    }
    else if( context.valueFound == 0U )
    {
        /* The response buffer is invalid as only the header field was found
         * in the "<field>: <value>\r\n" format of an HTTP header. */
        LogError( ( "Unable to find header value in response: "
                    "Response data is invalid: "
                    "RequestedHeader=%.*s, ParserError=%s",
                    ( int ) fieldLen,
                    pField,
                    http_errno_description( HTTP_PARSER_ERRNO( &( parser ) ) ) ) );
        returnStatus = HTTPInvalidResponse;
    }
    else
    {
        /* Header is found. */
        assert( ( context.fieldFound == 1U ) && ( context.valueFound == 1U ) );

        LogDebug( ( "Found requested header in response: "
                    "HeaderName=%.*s, HeaderValue=%.*s",
                    ( int ) fieldLen,
                    pField,
                    ( int ) ( *pValueLen ),
                    *pValueLoc ) );
        returnStatus = HTTPSuccess;
    }

    /* If the header field-value pair is found in response, then the return
     * value of "on_header_value" callback (related to the header value) should
     * cause the http_parser.http_errno to be "CB_header_value". */
    if( ( returnStatus == HTTPSuccess ) &&
        ( parser.http_errno != ( unsigned int ) HPE_CB_header_value ) )
    {
        LogError( ( "Header found in response but http-parser returned error: "
                    "ParserError=%s",
                    http_errno_description( HTTP_PARSER_ERRNO( &( parser ) ) ) ) );
        returnStatus = HTTPParserInternalError;
    }

    /* If header was not found, then the "on_header_complete" callback is
     * expected to be called which should cause the http_parser.http_errno to be
     * "OK" */
    else if( ( returnStatus == HTTPHeaderNotFound ) &&
             ( parser.http_errno != ( unsigned int ) ( HPE_OK ) ) )
    {
        LogError( ( "Header not found in response: http-parser returned error: "
                    "ParserError=%s",
                    http_errno_description( HTTP_PARSER_ERRNO( &( parser ) ) ) ) );
        returnStatus = HTTPInvalidResponse;
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

HTTPStatus_t HTTPClient_ReadHeader( const HTTPResponse_t * pResponse,
                                    const char * pField,
                                    size_t fieldLen,
                                    const char ** pValueLoc,
                                    size_t * pValueLen )
{
    HTTPStatus_t returnStatus = HTTPSuccess;

    if( pResponse == NULL )
    {
        LogError( ( "Parameter check failed: pResponse is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pResponse->pBuffer == NULL )
    {
        LogError( ( "Parameter check failed: pResponse->pBuffer is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pResponse->bufferLen == 0U )
    {
        LogError( ( "Parameter check failed: pResponse->bufferLen is 0: "
                    "Buffer len should be > 0." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pField == NULL )
    {
        LogError( ( "Parameter check failed: Input header name is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( fieldLen == 0U )
    {
        LogError( ( "Parameter check failed: Input header name length is 0: "
                    "fieldLen should be > 0." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pValueLoc == NULL )
    {
        LogError( ( "Parameter check failed: Output parameter for header value location is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else if( pValueLen == NULL )
    {
        LogError( ( "Parameter check failed: Output parameter for header value length is NULL." ) );
        returnStatus = HTTPInvalidParameter;
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    if( returnStatus == HTTPSuccess )
    {
        returnStatus = findHeaderInResponse( pResponse->pBuffer,
                                             pResponse->bufferLen,
                                             pField,
                                             fieldLen,
                                             pValueLoc,
                                             pValueLen );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

const char * HTTPClient_strerror( HTTPStatus_t status )
{
    const char * str = NULL;

    switch( status )
    {
        case HTTPSuccess:
            str = "HTTPSuccess";
            break;

        case HTTPInvalidParameter:
            str = "HTTPInvalidParameter";
            break;

        case HTTPNetworkError:
            str = "HTTPNetworkError";
            break;

        case HTTPPartialResponse:
            str = "HTTPPartialResponse";
            break;

        case HTTPNoResponse:
            str = "HTTPNoResponse";
            break;

        case HTTPInsufficientMemory:
            str = "HTTPInsufficientMemory";
            break;

        case HTTPSecurityAlertResponseHeadersSizeLimitExceeded:
            str = "HTTPSecurityAlertResponseHeadersSizeLimitExceeded";
            break;

        case HTTPSecurityAlertExtraneousResponseData:
            str = "HTTPSecurityAlertExtraneousResponseData";
            break;

        case HTTPSecurityAlertInvalidChunkHeader:
            str = "HTTPSecurityAlertInvalidChunkHeader";
            break;

        case HTTPSecurityAlertInvalidProtocolVersion:
            str = "HTTPSecurityAlertInvalidProtocolVersion";
            break;

        case HTTPSecurityAlertInvalidStatusCode:
            str = "HTTPSecurityAlertInvalidStatusCode";
            break;

        case HTTPSecurityAlertInvalidCharacter:
            str = "HTTPSecurityAlertInvalidCharacter";
            break;

        case HTTPSecurityAlertInvalidContentLength:
            str = "HTTPSecurityAlertInvalidContentLength";
            break;

        case HTTPParserInternalError:
            str = "HTTPParserInternalError";
            break;

        case HTTPHeaderNotFound:
            str = "HTTPHeaderNotFound";
            break;

        case HTTPInvalidResponse:
            str = "HTTPInvalidResponse";
            break;

        default:
            LogWarn( ( "Invalid status code received for string conversion: "
                       "StatusCode=%d", ( int ) status ) );
            break;
    }

    return str;
}

/*-----------------------------------------------------------*/
HTTPStatus_t HTTPClient_Request( const TransportInterface_t * pTransport,
                                HTTPRequestHeaders_t * pRequestHeaders,
                                const uint8_t * pRequestBodyBuf,
                                size_t reqBodyBufLen,
                                HTTPResponse_t * pResponse,
                                uint32_t sendFlags )
{
    HTTPStatus_t returnStatus = HTTPInvalidParameter;

    if( pResponse->getTime == NULL ) {
        /* Set a zero timestamp function when the application did not configure * one. */
        pResponse->getTime = getZeroTimestampMs;
    }

    returnStatus = sendHttpRequest( pTransport,
                                    pResponse->getTime,
                                    pRequestHeaders,
                                    pRequestBodyBuf,
                                    reqBodyBufLen,
                                    sendFlags );
    if (returnStatus != HTTPSuccess) {
        return returnStatus;
    }
    size_t totalReceived = 0U;
    int32_t currentReceived = 0;
    HTTPParsingContext_t parsingContext = { 0 };

    uint8_t *headerEOF  = NULL;
    int32_t  headerOff  = 0;
    int32_t  headerLen  = 0;
    size_t   bodyLen    = 0U;

    http_parser_settings parserSettings = { 0 };
    size_t bytesParsed = 0U;
    size_t   chunkLen    = 0;
    uint8_t *chunkBuffer = NULL;

    while (parsingContext.recvState != HTTP_RECV_DONE) {

        switch (parsingContext.recvState) {

        case HTTP_RECV_INIT:
            memset(pResponse, 0, sizeof(HTTPResponse_t));
            pResponse->pBuffer = HTTP_MALLOC(HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES + 1);
            if (NULL == pResponse->pBuffer) {
                return HTTPInsufficientMemory;
            }
            pResponse->bufferLen = HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES;
            parsingContext.pResponse  = pResponse;
            parsingContext.pBufferCur = ( const char * ) pResponse->pBuffer;
            http_parser_settings_init( &parserSettings );
            parserSettings.on_message_begin     = httpParserOnMessageBeginCallback;
            parserSettings.on_status            = httpParserOnStatusCallback;
            parserSettings.on_header_field      = httpParserOnHeaderFieldCallback;
            parserSettings.on_header_value      = httpParserOnHeaderValueCallback;
            parserSettings.on_headers_complete  = httpParserOnHeadersCompleteCallback;
            parserSettings.on_body              = httpParserOnBodyCallback;
            parserSettings.on_message_complete  = httpParserOnMessageCompleteCallback;
            parserSettings.on_chunk_header      = httpParserOnChunkHeaderCallback;
            parserSettings.on_chunk_complete    = httpParserOnChunkComleteCallback;
            parsingContext.httpParser.data      = &parsingContext;
            /* Initialize the third-party HTTP parser to parse responses. */
            http_parser_init(&parsingContext.httpParser, HTTP_RESPONSE);
            /* The parser will return an error if this header size limit is exceeded. */
            http_parser_set_max_header_size(HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES);
            parsingContext.recvState            = HTTP_RECV_HEADER;

        case HTTP_RECV_HEADER:
            currentReceived = pTransport->recv(pTransport->pNetworkContext,
                                               pResponse->pBuffer   + totalReceived,
                                               pResponse->bufferLen - totalReceived);
            if (currentReceived <= 0) {
                LogError( ( "Failed to receive HTTP data: Transport recv() "
                            "returned error: TransportStatus=%ld",
                            ( long int ) currentReceived ) );
                return HTTPNetworkError;
            }
            totalReceived += currentReceived;
            pResponse->pBuffer[totalReceived] = 0;
            headerEOF = strstr(&pResponse->pBuffer[headerOff], "\r\n\r\n");
            if (headerEOF) {
                headerLen = (headerEOF + 4) - pResponse->pBuffer;
                bodyLen   = totalReceived - headerLen;
                parsingContext.recvState = HTTP_PARSE_HEADER;
                break;
            }
            if (totalReceived >= HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES) {
                returnStatus = HTTPInsufficientMemory;
                goto __exit;
            } else {
                //! next header offset
                headerOff = totalReceived > 4 ? totalReceived - 4 : 0;
            }
            break;

        case HTTP_PARSE_HEADER:
            bytesParsed  = http_parser_execute(&(parsingContext.httpParser),
                                               &parserSettings,
                                               pResponse->pBuffer,
                                               headerLen);
            returnStatus = processHttpParserError(&(parsingContext.httpParser));
            if (HPE_OK != returnStatus) {
                goto __exit;
            }
            if (0 == pResponse->contentLength && !(parsingContext.httpParser.flags & F_CHUNKED)) {
                returnStatus = HTTPSecurityAlertInvalidContentLength;
                goto __exit;
            }
            if (sendFlags && HTTP_SEND_DISABLE_RECV_BODY_FLAG) {
                if (0 == bodyLen) {
                    return OPRT_OK;
                }
                //! Remaining length of the body in the header
                pResponse->bodyLen = bodyLen;
                pResponse->pBody   = HTTP_MALLOC(pResponse->bodyLen + 1);
                if (NULL == pResponse->pBody) {
                    returnStatus = HTTPInsufficientMemory;
                    goto __exit;
                }
                memcpy(pResponse->pBody, pResponse->pBuffer + headerLen, bodyLen);
                return OPRT_OK;
            }
            if (parsingContext.httpParser.flags & F_CHUNKED) {
                chunkBuffer = HTTP_MALLOC(HTTP_MAX_RESPONSE_CHUNK_ONCE_BYTES + 1);
                if (NULL == chunkBuffer) {
                    returnStatus = HTTPInsufficientMemory;
                    goto __exit;
                }
                pResponse->pBody = HTTP_MALLOC(HTTP_MAX_RESPONSE_CHUNK_SIZE_BYTES + 1);
                parsingContext.recvState = HTTP_RECV_CHUNK;
            } else {
                pResponse->pBody = HTTP_MALLOC(pResponse->contentLength + 1);
                parsingContext.recvState = HTTP_RECV_BODY;
            }
            if (NULL == pResponse->pBody) {
                returnStatus = HTTPInsufficientMemory;
                goto __exit;
            }
            //! Remaining length of the body in the header
            if (bodyLen) {
                if (parsingContext.httpParser.flags & F_CHUNKED) {
                    memcpy(chunkBuffer, pResponse->pBuffer + headerLen, bodyLen);
                    chunkLen = bodyLen;
                    parsingContext.recvState = HTTP_PARSE_CHUNK;
                } else {
                    memcpy(pResponse->pBody, pResponse->pBuffer + headerLen, bodyLen);
                    if (bodyLen == pResponse->contentLength) {
                        parsingContext.recvState = HTTP_PARSE_BODY;
                    }
                }
            }
            break;

        case HTTP_RECV_CHUNK:
            currentReceived = pTransport->recv( pTransport->pNetworkContext,
                                                chunkBuffer,
                                                HTTP_MAX_RESPONSE_CHUNK_ONCE_BYTES);
            if (currentReceived <= 0) {
                LogError( ( "Failed to receive HTTP data: Transport recv() "
                            "returned error: TransportStatus=%ld",
                            ( long int ) currentReceived ) );
                return HTTPNetworkError;
            }
            chunkLen = currentReceived;
            parsingContext.recvState = HTTP_PARSE_CHUNK;


        case HTTP_PARSE_CHUNK:
            bytesParsed  = http_parser_execute(&parsingContext.httpParser,
                                               &parserSettings,
                                               chunkBuffer,
                                               chunkLen);
            returnStatus = processHttpParserError(&(parsingContext.httpParser));
            if (HPE_OK != returnStatus) {
                goto __exit;
            }
            LogDebug( ( "Parsed HTTP Response buffer: BytesParsed=%lu, "
                        "ExpectedBytesParsed=%lu",
                        ( unsigned long ) bytesParsed,
                        ( unsigned long ) chunkLen ) );
            if (parsingContext.recvState != HTTP_RECV_DONE) {
                parsingContext.recvState = HTTP_RECV_CHUNK;
            }
            break;

        case HTTP_RECV_BODY:
            currentReceived = pTransport->recv( pTransport->pNetworkContext,
                                                pResponse->pBody + bodyLen,
                                                pResponse->contentLength - bodyLen);
            if (currentReceived <= 0) {
                LogError( ( "Failed to receive HTTP data: Transport recv() "
                            "returned error: TransportStatus=%ld",
                            ( long int ) currentReceived ) );
                return HTTPNetworkError;
            }
            bodyLen += currentReceived;
            if (pResponse->contentLength == bodyLen) {
                parsingContext.recvState = HTTP_PARSE_BODY;
            }
            break;

        case HTTP_PARSE_BODY:
            bytesParsed  = http_parser_execute(&parsingContext.httpParser,
                                               &parserSettings,
                                               pResponse->pBody,
                                               bodyLen);
            returnStatus = processHttpParserError(&(parsingContext.httpParser));
            if (HPE_OK != returnStatus) {
                goto __exit;
            }
            if (bytesParsed == pResponse->contentLength) {  
                parsingContext.recvState = HTTP_RECV_DONE;
            }
        }
    }

    if (chunkBuffer) {
        HTTP_FREE(chunkBuffer);
    }

    return returnStatus;

__exit:
    if (chunkBuffer) {
        HTTP_FREE(chunkBuffer);
    }

    if (pResponse->pBuffer) {
        HTTP_FREE(pResponse->pBuffer);
    }

    if (pResponse->pBody) {
        HTTP_FREE(pResponse->pBody);
    }

    return returnStatus;
}



int32_t HTTPClient_Recv(const TransportInterface_t *pTransport,
                        HTTPResponse_t *pResponse,
                        uint8_t *data,
                        size_t   dataLen)
{
    int32_t currentReceived = 0;
    size_t copyLen = 0U;

    if( ( data == NULL ) || ( dataLen == 0U ) )
    {
        LogError( ( "Parameter check failed: data is NULL or dataLen is 0." ) );
        return -( int32_t ) HTTPInvalidParameter;
    }

    if (pResponse->pBody && pResponse->bodyLen) {
        copyLen = ( pResponse->bodyLen < dataLen ) ? pResponse->bodyLen : dataLen;

        if( copyLen > ( size_t ) INT32_MAX )
        {
            LogError( ( "Receive buffer length exceeds int32_t capacity: CopyLen=%lu",
                        ( unsigned long ) copyLen ) );
            return -( int32_t ) HTTPInsufficientMemory;
        }

        memcpy( data, pResponse->pBody, copyLen );
        currentReceived = ( int32_t ) copyLen;

        if( pResponse->bodyLen > copyLen )
        {
            size_t remainingLen = pResponse->bodyLen - copyLen;

            ( void ) memmove( ( uint8_t * ) pResponse->pBody,
                              pResponse->pBody + copyLen,
                              remainingLen );
            pResponse->bodyLen = remainingLen;
            return currentReceived;
        }

        HTTP_FREE(pResponse->pBody);
        pResponse->pBody = NULL;
        pResponse->bodyLen = 0;
        return currentReceived;
    }

    currentReceived = pTransport->recv( pTransport->pNetworkContext,
                                        data,
                                        dataLen);

    if (currentReceived <= 0) {
        LogError( ( "Failed to receive HTTP data: Transport recv() "
                    "returned error: TransportStatus=%ld",
                    ( long int ) currentReceived ) );
    }


    return currentReceived;
}
