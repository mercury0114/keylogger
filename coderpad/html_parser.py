import html2text
import json
import os

BEGIN_STRING = "// CODE"
END_STRING = "// DISCUSSION"
HTML_FILE = "data/page.html"
OUTPUT_FILE = "data/extracted_code.cc"

f = open(HTML_FILE)
html = f.read()
f.close()

h = html2text.HTML2Text()
h.ignore_links = True
text = h.handle(html)

codeBegin = text.find(BEGIN_STRING)
if (codeBegin == -1):
    print("ERROR: found no " + BEGIN_STRING)
    exit()
codeEnd = text.find(END_STRING)
if (codeEnd == -1):
    print("ERROR: found no " + END_STRING)
    exit()

lines = text[codeBegin:codeEnd].split('\n')
codeLines = [line for line in lines \
             if line and not line.isspace() and not line.isnumeric()]

if (os.path.exists(OUTPUT_FILE)):
    os.remove(OUTPUT_FILE)
f = open(OUTPUT_FILE, "w")
for line in codeLines:
    f.write(line.encode("ascii", "ignore").decode() + '\n')
f.close()
