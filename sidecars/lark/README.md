# Feishu WebSocket sidecar

This sidecar connects to Feishu through the `lark-oapi` WebSocket client and
receives `im.message.receive_v1` bot events. It does not need a public callback
URL.

## Feishu setup

1. Create an enterprise self-built app and enable the bot capability.
2. Add the required message permissions.
3. Under **Events and callbacks**, select **Receive events through a persistent
   connection**.
4. Subscribe to **Receive message** (`im.message.receive_v1`) and publish the
   app version.

To let the bot send pairing codes back to users, also enable the bot message
sending permission in the app permission page, then publish the app version
again.

## Run

Python 3.8 or later is required.

```bash
cd sidecars/lark
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

export LARK_APP_ID=cli_xxxxxxxxxxxxx
export LARK_APP_SECRET=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
python main.py
```

The WebSocket connection is outbound, so the process must be able to access
Feishu over the internet.

## Handle or forward events

Edit `handle_event()` in `handler.py` to add in-process bot behavior.

The sidecar uses a local permission file to bind Lark `open_id` values to
system users. Unknown users are not forwarded to downstream services. Instead,
the process writes a pending pairing code to `lark_permissions.json`, logs it,
and sends it back to the current Lark chat when message sending is permitted.

Create the first permission file from the example:

```bash
cp lark_permissions.example.json lark_permissions.json
```

When an unknown user sends a message, logs look like:

```text
unauthorized Lark user: open_id=ou_xxx tenant_key=tenant_xxx pairing_code=AB12CD34
```

Approve the code locally:

```bash
python pairing.py --file lark_permissions.json approve AB12CD34 alice --roles admin
```

The sidecar reloads the permission file periodically. You can tune this with:

```bash
export LARK_PERMISSION_FILE=lark_permissions.json
export LARK_PERMISSION_RELOAD_SECONDS=30
export LARK_PAIRING_TTL_SECONDS=600
```

To forward authorized text messages to the C++ AgentServer:

```bash
export LARK_FORWARD_URL=http://127.0.0.1:8080/agent/requests
export LARK_FORWARD_TOKEN=optional-bearer-token
python main.py
```

The downstream service receives a channel-neutral agent request:

```json
{
  "version": "1",
  "request_id": "lark:event-id",
  "source": {
    "platform": "lark",
    "tenant_id": "tenant-key",
    "user_id": "ou_xxx",
    "chat_id": "oc_xxx",
    "message_id": "om_xxx"
  },
  "message": {
    "type": "text",
    "text": "hello"
  },
  "auth": {
    "system_user_id": "alice",
    "roles": ["admin"]
  },
  "reply": {
    "platform": "lark",
    "target_type": "chat",
    "target_id": "oc_xxx"
  }
}
```

The AgentServer executes the message and replies through its configured
channel responder. Lark replies are sent directly from the C++ process through
the Lark HTTP API.
