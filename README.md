# jmidiplay

JACK MIDI file player (via [libsmf](http://libsmf.sourceforge.net/))

usage: `jmidiplay [-vh] input:port file_name`

example: `jmidiplay qsynth:midi_00 music.mid`

# jmidirec

JACK MIDI file recorder (via [libsmf](http://libsmf.sourceforge.net/))

usage: `jmidirec [-vh] output:port file_name`

example: `jmidirec jack-keyboard:midi_out save.mid`
