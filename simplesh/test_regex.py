import re

regex = r"^(?: *([^\s\|]+) *)+(?:\| *([^\s\|]+) *)*\n"

m = re.match(regex, '  wtf 1 arg2   arg | next|last    \n')
print(m, m.groups()[0])
print(m.groups())

# (?P<name>...)
# (?P=name)
