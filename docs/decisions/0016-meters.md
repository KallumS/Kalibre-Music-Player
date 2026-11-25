# 0016: The meters read what's heard

Accepted, 25 November 2026.

The audio thread copies its output, after the volume, into a ring buffer the visualisers read (oscilloscope, goniometer, spectrum), sets peak and RMS atomics (peak meter), and runs Kalibre's R128 meter (loudness). A visualiser hidden in a tab not shown does nothing.
