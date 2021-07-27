import os
import sys

print("20 text/gemini\r")

print("Hello world! I am being written from a cgi script!\n")

print("Below is a list of the environment variables passed to the script:\n\n```")

for k, v in os.environ.items():
    print("{}={}".format(k, v))

print("```\n")

print("And here are the arguments that were given to start the script:\n\n```")

for arg in sys.argv:
    print(repr(arg))

print("```\n")