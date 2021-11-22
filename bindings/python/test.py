import sys
import ggwave

testFailed = False

ggwave.disableLog()
ggwave.enableLog()

samples = ggwave.encode("hello python")

if not (samples):
    testFailed = True

if testFailed:
    print("Some of the tests failed!")
else:
    print("All tests passed!")

sys.exit(testFailed)
