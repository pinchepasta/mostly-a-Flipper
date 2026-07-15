T-Embed XRemote patch

What this patch changes:
- Turning the T-Embed wheel only moves the selected highlight.
- Pressing the wheel center sends only the selected IR command.
- The side Back button returns/exits XRemote as before.
- Missing commands in normal .ir files no longer cause a null-pointer crash.
- The patch does not alter your remote files or IR codes.

Install by copying the flipper_xremote folder from this ZIP over:
applications_user/flipper_xremote
inside your Flipper-Zero-ESP32-Port repo, then build:
./buildFap.sh applications_user/flipper_xremote

This is a first stability/control patch. Once it runs reliably, use the actual LG .ir file to add command-name aliases or rename only the name: fields as needed.
