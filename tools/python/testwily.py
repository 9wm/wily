#!/usr/local/bin/python -u
# -u == unbuffered output

import wily
print 'get connection'
c = wily.Connection()

print 'get window "fish"'
w = c.win('fish', 1)
print 'new window', w

print 'get list of windows'
print 'listing', c.list()

print 'change name from "fish" to "newname"'
c.setname(w, 'newname')

print 'insert some text'
c.replace(w, 0, 0, 'pack my box with five dozen liquor jugs')

print 'search for the text "box"'
w2,f,t  =  c.goto(w, 0, 0, 'box', 0)
print 'found at', w2, f, t

print 'replace "box" with "crate"'
c.replace(w, f, t, 'crate')

print 'search for the whole window'
w2,f,t  =  c.goto(w, 0, 0, ':,', 0)
print 'whole file is', w2, f, t

print 'replace the whole window with another phrase'
c.replace(w, f, t, 'the quick brown fox jumped over the lazy dog')

print 'read the range [10,15) from the window'
#s = c.read(w, 10, 15)
# print 'read ', s

print 'attach to the window, grab EXEC messages only'
c.attach(w, wily.EXEC)

print 'print the next ten events'
for j in range(10):
	print 'waiting'
	print 'got', c.event()

print 'done'
