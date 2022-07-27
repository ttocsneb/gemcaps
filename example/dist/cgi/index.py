#!/usr/bin/env python3

import os
import sys
from pprint import pformat



environ = "\n".join([f"{key}={val}" for key, val in os.environ.items()])
args = "\n".join(sys.argv)

path_info = os.environ.get('PATH_INFO', '')
query = os.environ.get('QUERY_STRING', '')
if query:
    query = '?' + query

print("20 text/gemini\r")
print(f"""
# The Common Gateway Interface for Gemini                                                                                             

=> / 🏠 home
                                                                                                                                                              
The Common Gateway Interface can easily be adapted for the new Gemini                                                                                
protocol.  In fact, the document you are reading right now is genreated from                                                                           
a program via the Common Gateway Interface, aka CGI.  The specification for                                                                            
CGI, RFC-3875, is pretty straightforward, and if you strip out the HTTP                                                                                
specific bits, it was rather easy to implement.                                                                                                        
                                                                                                                                                              
CGI Environment Variables:

```
{environ}
```

CGI Arguments:

```
{args}
```

> This page is taken from
=> gemini://gemini.conman.org/cgi{path_info}{query} gemini.conman.org's cgi example
""")
