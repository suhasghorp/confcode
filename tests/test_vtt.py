from confcode.download import parse_vtt

VTT = """WEBVTT
Kind: captions

00:00:01.000 --> 00:00:03.000 align:start position:0%
so<00:00:01.500><c> senders</c><00:00:02.000><c> and receivers</c>

00:00:03.000 --> 00:00:03.010 align:start position:0%
so senders and receivers

00:00:03.010 --> 00:00:05.000 align:start position:0%
so senders and receivers
are &amp; composable
"""


def test_rolling_captions_deduped():
    segs = parse_vtt(VTT)
    assert [s.text for s in segs] == ["so senders and receivers", "are & composable"]
    assert segs[0].start == 1.0
