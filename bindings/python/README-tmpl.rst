..  [[[cog

    import cog
    import ggwave

    def indent(text, indentation = "    "):
        return indentation + text.replace("\n", "\n" + indentation)

    def comment(text):
        return "# " + text.replace("\n", "\n# ")

    def cogOutExpression(expr):
        cog.outl(indent(expr))
        cog.outl(indent(comment(str(eval(expr)))))

    ]]]
    [[[end]]]

======
ggwave
======

Tiny data-over-sound library.

..  [[[cog

    cog.outl()
    cog.outl(".. code:: python")
    cog.outl()

    cog.outl(indent('waveform = ggwave.encode("hello python")'))
    cog.outl()

    waveform = ggwave.encode("hello python")

    ]]]

.. code::

   {{ Basic code examples will be generated here. }}

..  [[[end]]]

--------
Features
--------

* Audible and ultrasound transmissions available
* Bandwidth of 8-16 bytes/s (depending on the transmission protocol)

------------
Installation
------------
::

    pip install ggwave

---
API
---

encode()
--------

.. code:: python

    encode(data)

Encodes ``data`` into a sound waveform.

..  [[[cog

    import pydoc

    help_str = pydoc.plain(pydoc.render_doc(ggwave.encode, "%s"))

    cog.outl()
    cog.outl('Output of ``help(ggwave.encode)``:')
    cog.outl()
    cog.outl('.. code::\n')
    cog.outl(indent(help_str))

    ]]]

.. code::

   {{ Content of help(ggwave.encode) will be generated here. }}

..  [[[end]]]

decode()
--------

TODO

-----
Usage
-----

.. code:: python

    import pyaudio
    import numpy as np

    import ggwave

    p = pyaudio.PyAudio()

    waveform = ggwave.encode("hello python")

    stream = p.open(format=pyaudio.paInt16, channels=1, rate=48000, output=True, frames_per_buffer=4096)
    stream.write(np.array(waveform).astype(np.int16), len(waveform))
    stream.stop_stream()
    stream.close()

    p.terminate()

----
More
----

Check out `<http://github.com/ggerganov/ggwave>`_ for more information about ggwave!

-----------
Development
-----------

Check out `ggwave python package on Github <https://github.com/ggerganov/ggwave/tree/master/bindings/python>`_.
