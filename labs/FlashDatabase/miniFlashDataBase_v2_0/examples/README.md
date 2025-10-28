# Upload Examples

Two simple sketches show how to hook `UploadHelpers` into different transports:

- `upload_http.cpp` demonstrates sending NDJSON batches with HTTP POST,
  including an `X-Idempotency-Key` header derived from the record sequence.
- `upload_mqtt.cpp` publishes CSV payloads to MQTT, using the sequence number as
  part of the topic for idempotency.

Both examples assume Wi-Fi/MQTT is already connected elsewhere in your sketch.
Call `uploadLogsOverHttp(logger)` or `uploadLogsOverMqtt(logger)` from your main
loop to ship pending records. Adjust the policy struct to tune retries/backoff.

## Coming Soon

- CSV chart pipeline (export to SD card / host tooling).
- Shell walkthrough script (automated CLI transcript).
- BLE upload stub mirroring the HTTP/MQTT helper flow.
