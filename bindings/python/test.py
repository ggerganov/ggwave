import sys
import ggwave

testFailed = False

n, samples = ggwave.encode("hello python")

if not (samples and n > 1024):
    testFailed = True

if testFailed:
    print("Some of the tests failed!")
else:
    print("All tests passed!")

sys.exit(testFailed)
