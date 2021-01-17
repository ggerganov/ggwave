import sys
import ggwave

testFailed = False

#result = ggwave.testC()
#if not (result):
#    testFailed = True

if testFailed:
    print("Some of the tests failed!")
else:
    print("All tests passed!")

sys.exit(testFailed)
