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

Possible applications:

- Serverless, one-to-many broadcast
- Device pairing
- Authorization
- Internet of Things
- Audio QR codes

..  [[[cog

    cog.outl()
    cog.outl(".. code:: python")
    cog.outl()

    cogOutExpression('ggwave.ggwaveTest()')
    cog.outl()

    ]]]

.. code::

   {{ Basic code examples will be generated here. }}

..  [[[end]]]

ggwave is actually a C/C++ library, and this package is it's wrapper for Python.

--------
Features
--------

* TODO

------------
Installation
------------
::

    pip install ggwave

---
API
---

TODO

----
More
----

Check out `C/C++ ggwave docs <http://github.com/ggerganov/ggwave>`_ for more information about ggwave!

-----------
Development
-----------

Check out `ggwave python package on Github <https://github.com/ggerganov/ggwave/tree/master/bindings/python>`_.
