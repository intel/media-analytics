# Copyright (c) 2020 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# This is simple Makefile to build and install demo manual pages. It is supposed
# to be run under docker build, so we skip comprehensive configure stage for now.

INSTALL=install

prefix?=$(HOME)
mandir?=$(prefix)/share/man
man1dir=$(mandir)/man1

MAN:=
MAN+=demo-bash.asciidoc
MAN+=demo.env.asciidoc
MAN+=demo-setup.asciidoc
MAN+=demo-alive.asciidoc
MAN+=ObjectDetection.asciidoc
MAN+=ObjectClassification.asciidoc
MAN+=SamplePipeline.asciidoc

DOC_MAN1=$(patsubst %.asciidoc,%.1,$(MAN))

all: $(DOC_MAN1)

%.1: %.asciidoc
	a2x --format manpage --no-xmllint $<

clean:
	$(RM) *.1

install: all
	$(INSTALL) -d -m 755 $(DESTDIR)$(man1dir)
	$(INSTALL) -m 644 $(DOC_MAN1) $(DESTDIR)$(man1dir)
