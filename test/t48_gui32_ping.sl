gui32 = import("../modules/gui32")
app = gui32.connect(null)

print(app.ping())
app.shutdown()

print("=== GUI32 PING PASSED ===")
