#include "tuya_error_code.h"
#include "core_mqtt_config.h"
#include "core_mqtt.h"

#include "mqtt_client_interface.h"

#include "tuya_tls.h"
#include "tuya_transporter.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_memory.h"

#define log_debug PR_DEBUG
#define log_error PR_ERR

typedef struct {
    mqtt_client_config_t config;
    MQTTContext_t mqclient;
    tuya_transporter_t network;
    uint8_t mqttbuffer[CORE_MQTT_BUFFER_SIZE];
} mqtt_client_context_t;

static void core_mqtt_library_callback(struct MQTTContext *pContext, struct MQTTPacketInfo *pPacketInfo,
                                       struct MQTTDeserializedInfo *pDeserializedInfo)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)pContext->userData;

    uint16_t msgid = pDeserializedInfo->packetIdentifier;

    /* Handle incoming publish. The lower 4 bits of the publish packet
     * type is used for the dup, QoS, and retain flags. Hence masking
     * out the lower bits to check if the packet is publish. */
    if ((pPacketInfo->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH) {
        if (context->config.on_message == NULL) {
            return;
        }

        char *topic = tal_malloc(pDeserializedInfo->pPublishInfo->topicNameLength + 1);
        if (topic == NULL) {
            return;
        }
        memcpy(topic, pDeserializedInfo->pPublishInfo->pTopicName, pDeserializedInfo->pPublishInfo->topicNameLength);
        topic[pDeserializedInfo->pPublishInfo->topicNameLength] = '\0';

        context->config.on_message(context, msgid,
                                   &(const mqtt_client_message_t){
                                       .topic = topic,
                                       .payload = pDeserializedInfo->pPublishInfo->pPayload,
                                       .length = pDeserializedInfo->pPublishInfo->payloadLength,
                                       .qos = pDeserializedInfo->pPublishInfo->qos,
                                   },
                                   context->config.userdata);
        tal_free(topic);

    } else {
        switch (pPacketInfo->type) {
        case MQTT_PACKET_TYPE_SUBACK:
            log_debug("MQTT_PACKET_TYPE_SUBACK id:%d", msgid);
            if (context->config.on_subscribed) {
                context->config.on_subscribed(context, msgid, context->config.userdata);
            }
            break;

        case MQTT_PACKET_TYPE_UNSUBACK:
            log_debug("MQTT_PACKET_TYPE_UNSUBACK id:%d", msgid);
            if (context->config.on_unsubscribed) {
                context->config.on_unsubscribed(context, msgid, context->config.userdata);
            }
            break;

        case MQTT_PACKET_TYPE_PUBACK:
            log_debug("MQTT_PACKET_TYPE_PUBACK id:%d", msgid);
            if (context->config.on_published) {
                context->config.on_published(context, msgid, context->config.userdata);
            }
            break;

        default:
            log_debug("type:0x%02x, id:%d", pPacketInfo->type, msgid);
        }
    }
}

void *mqtt_client_new(void)
{
    return tal_malloc(sizeof(mqtt_client_context_t));
}

void mqtt_client_free(void *client)
{
    tal_free(client);
}

static int network_write(NetworkContext_t *pNetwork, const unsigned char *pMsg, size_t len)
{
    tuya_transporter_t transporter = *pNetwork;

    return tuya_transporter_write(transporter, (uint8_t *)pMsg, len, 0);
}

static int network_read(NetworkContext_t *pNetwork, unsigned char *pMsg, size_t len)
{
    tuya_transporter_t transporter = *pNetwork;

    tuya_tls_config_t *tls_config = NULL;

    tuya_transporter_ctrl(transporter, TUYA_TRANSPORTER_GET_TLS_CONFIG, &tls_config);

    int timeout = tls_config ? tls_config->timeout : 5000;

    int result = tuya_transporter_read(transporter, (uint8_t *)pMsg, len, timeout);

    if (result == OPRT_RESOURCE_NOT_READY) {
        return 0;
    }

    return result;
}
static uint32_t __mqtt_client_get_current_time(void)
{
    return (uint32_t)tal_system_get_millisecond();
}
mqtt_client_status_t mqtt_client_init(void *client, const mqtt_client_config_t *config)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    /* Clean memory */
    memset(context, 0, sizeof(mqtt_client_context_t));

    /* Setting data */
    TUYA_TRANSPORT_TYPE_E transport_type = (config->cacert == NULL) ? TRANSPORT_TYPE_TCP : TRANSPORT_TYPE_TLS;
    context->config = *config;
    context->network = tuya_transporter_create(transport_type, NULL);
    if (NULL == context->network) {
        return MQTT_STATUS_NETWORK_INIT_FAILED;
    }
    if (transport_type == TRANSPORT_TYPE_TLS) {
        tuya_tls_config_t tls_config = {
            .ca_cert = (char *)context->config.cacert,
            .ca_cert_size = context->config.cacert_len,
            .hostname = (char *)context->config.host,
            .port = context->config.port,
            .timeout = context->config.timeout_ms,
            .mode = TUYA_TLS_SERVER_CERT_MODE,
            .verify = true,
        };

        int ret = tuya_transporter_ctrl(context->network, TUYA_TRANSPORTER_SET_TLS_CONFIG, &tls_config);
        if (OPRT_OK != ret) {
            log_error("network_tls_init fail:%d", ret);
            return MQTT_STATUS_NETWORK_INIT_FAILED;
        }
    }

    /* Fill in TransportInterface send and receive function pointers.
     * For this demo, TCP sockets are used to send and receive data
     * from network. Network context is SSL context for OpenSSL.*/
    TransportInterface_t transport;
    transport.pNetworkContext = &context->network;
    transport.send = (TransportSend_t)network_write;
    transport.recv = (TransportRecv_t)network_read;

    /* Fill the values for network buffer. */
    MQTTFixedBuffer_t network_buffer;
    network_buffer.size = CORE_MQTT_BUFFER_SIZE;
    network_buffer.pBuffer = context->mqttbuffer;

    /* Initialize MQTT library. */
    mqtt_status = MQTT_Init(&context->mqclient, &transport, (MQTTGetCurrentTimeFunc_t)__mqtt_client_get_current_time,
                            core_mqtt_library_callback, &network_buffer, context);

    if (mqtt_status != MQTTSuccess) {
        log_error("MQTT init failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
        tuya_transporter_close(context->network);
        tuya_transporter_destroy(context->network);
        return OPRT_COM_ERROR;
    }

    return MQTT_STATUS_SUCCESS;
}

mqtt_client_status_t mqtt_client_deinit(void *client)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;

    tuya_transporter_close(context->network);
    tuya_transporter_destroy(context->network);
    return MQTT_STATUS_SUCCESS;
}

mqtt_client_status_t mqtt_client_connect(void *client)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    int ret = tuya_transporter_connect(context->network, context->config.host, context->config.port,
                                       context->config.timeout_ms);
    if (OPRT_OK != ret) {
        tuya_transporter_close(context->network);
        return MQTT_STATUS_NETWORK_CONNECT_FAILED;
    }

    bool pSessionPresent = false;

    /* Send MQTT CONNECT packet to broker. */
    mqtt_status = MQTT_Connect(&context->mqclient,
                               &(const MQTTConnectInfo_t){.cleanSession = true,
                                                          .keepAliveSeconds = context->config.keepalive,
                                                          .pClientIdentifier = context->config.clientid,
                                                          .clientIdentifierLength = strlen(context->config.clientid),
                                                          .pUserName = context->config.username,
                                                          .userNameLength = strlen(context->config.username),
                                                          .pPassword = context->config.password,
                                                          .passwordLength = strlen(context->config.password)},
                               NULL, context->config.timeout_ms, &pSessionPresent);
    if (MQTTSuccess != mqtt_status) {
        log_error("mqtt connect err: %s(%d)", MQTT_Status_strerror(mqtt_status), mqtt_status);
        tuya_transporter_close(context->network);
        if (MQTTNotAuthorized == mqtt_status) {
            return MQTT_STATUS_NOT_AUTHORIZED;
        }
        return MQTT_STATUS_CONNECT_FAILED;
    }

    if (context->config.on_connected) {
        context->config.on_connected(context, context->config.userdata);
    }

    return MQTT_STATUS_SUCCESS;
}

mqtt_client_status_t mqtt_client_disconnect(void *client)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    mqtt_status = MQTT_Disconnect(&context->mqclient);
    if (MQTTSuccess != mqtt_status) {
        log_error("mqtt disconnect err: %s(%d)", MQTT_Status_strerror(mqtt_status), mqtt_status);
    }

    tuya_transporter_close(context->network);

    if (context->config.on_disconnected) {
        context->config.on_disconnected(context, context->config.userdata);
    }

    return MQTT_STATUS_SUCCESS;
}

uint16_t mqtt_client_subscribe(void *client, const char *topic, uint8_t qos)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    uint16_t msgid = MQTT_GetPacketId(&context->mqclient);

    mqtt_status = MQTT_Subscribe(
        &context->mqclient,
        &(const MQTTSubscribeInfo_t){.qos = qos, .pTopicFilter = topic, .topicFilterLength = strlen(topic)}, 1, msgid);

    if (mqtt_status != MQTTSuccess) {
        log_error("Failed to send SUBSCRIBE packet to broker with error = %s.", MQTT_Status_strerror(mqtt_status));
        return 0;
    }

    return msgid;
}

uint16_t mqtt_client_unsubscribe(void *client, const char *topic, uint8_t qos)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    uint16_t msgid = MQTT_GetPacketId(&context->mqclient);

    mqtt_status = MQTT_Unsubscribe(
        &context->mqclient,
        &(const MQTTSubscribeInfo_t){.qos = qos, .pTopicFilter = topic, .topicFilterLength = strlen(topic)}, 1, msgid);

    if (mqtt_status != MQTTSuccess) {
        log_error("Failed to send SUBSCRIBE packet to broker with error = %s.", MQTT_Status_strerror(mqtt_status));
        return 0;
    }

    return msgid;
}

uint16_t mqtt_client_publish(void *client, const char *topic, const uint8_t *payload, size_t length, uint8_t qos)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    uint16_t msgid = MQTT_GetPacketId(&context->mqclient);

    mqtt_status = MQTT_Publish(&context->mqclient,
                               &(const MQTTPublishInfo_t){.qos = qos,
                                                          .pTopicName = topic,
                                                          .topicNameLength = strlen(topic),
                                                          .pPayload = payload,
                                                          .payloadLength = length},
                               msgid);

    if (MQTTSuccess != mqtt_status) {
        return 0;
    }
    return msgid;
}

mqtt_client_status_t mqtt_client_yield(void *client)
{
    mqtt_client_context_t *context = (mqtt_client_context_t *)client;
    MQTTStatus_t mqtt_status;

    mqtt_status = MQTT_ProcessLoop(&context->mqclient, context->config.timeout_ms);
    if (mqtt_status != MQTTSuccess) {
        log_error("MQTT_ProcessLoop returned with status = %s.", MQTT_Status_strerror(mqtt_status));
        mqtt_client_disconnect(context);
        return MQTT_STATUS_NETWORK_TIMEOUT;
    }
    return MQTT_STATUS_SUCCESS;
}