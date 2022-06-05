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

    cog.outl(indent(comment('generate audio waveform for string "hello python"')))
    cog.outl(indent('waveform = ggwave.encode("hello python")'))
    cog.outl()

    cog.outl(indent(comment('decode audio waveform')))
    cog.outl(indent('text = ggwave.decode(instance, waveform)'))
    cog.outl()

    ]]]

.. code::

   {{ Basic code examples will be generated here. }}

..  [[[end]]]

--------
Features
--------

* Audible and ultrasound transmissions available
* Bandwidth of 8-16 bytes/s (depending on the transmission protocol)
* Robust FSK modulation
* Reed-Solomon based error correction

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

    encode(payload, [protocolId], [volume], [instance])

Encodes ``payload`` into an audio waveform.

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

.. code:: python

    decode(instance, waveform)

Analyzes and decodes ``waveform`` into to try and obtain the original payload.
A preallocated ggwave ``instance`` is required.

..  [[[cog

    import pydoc

    help_str = pydoc.plain(pydoc.render_doc(ggwave.decode, "%s"))

    cog.outl()
    cog.outl('Output of ``help(ggwave.decode)``:')
    cog.outl()
    cog.outl('.. code::\n')
    cog.outl(indent(help_str))

    ]]]

.. code::

   {{ Content of help(ggwave.decode) will be generated here. }}

..  [[[end]]]


-----
Usage
-----

* Encode and transmit data with sound:

.. code:: python

    import ggwave
    import pyaudio

    p = pyaudio.PyAudio()

    # generate audio waveform for string "hello python"
    waveform = ggwave.encode("hello python", protocolId = 1, volume = 20)

    print("Transmitting text 'hello python' ...")
    stream = p.open(format=pyaudio.paFloat32, channels=1, rate=48000, output=True, frames_per_buffer=4096)
    stream.write(waveform, len(waveform)//4)
    stream.stop_stream()
    stream.close()

    p.terminate()

* Capture and decode audio data:

.. code:: python

    import ggwave
    import pyaudio

    p = pyaudio.PyAudio()

    stream = p.open(format=pyaudio.paFloat32, channels=1, rate=48000, input=True, frames_per_buffer=1024)

    print('Listening ... Press Ctrl+C to stop')
    instance = ggwave.init()

    try:
        while True:
            data = stream.read(1024, exception_on_overflow=False)
            res = ggwave.decode(instance, data)
            if (not res is None):
                try:
                    print('Received text: ' + res.decode("utf-8"))
                except:
                    pass
    except KeyboardInterrupt:
        pass

    ggwave.free(instance)

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
