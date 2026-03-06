import asyncio
import json
import websockets

async def chat():
    # get your device ip address
    uri = "ws://xxx.xxx.xxx.xxx:18789"
    async with websockets.connect(uri) as ws:
        # send message
        await ws.send(json.dumps({
            "type": "message",
            "content": "What is your name?",
            "chat_id": "python_test"
        }))

        # wait for response
        resp = await ws.recv()
        data = json.loads(resp)
        print("AI reply:", data["content"])

asyncio.run(chat())