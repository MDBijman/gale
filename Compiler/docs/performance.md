### Performance Targets

The compiler should be able to compile code fast enough to ensure a smooth edit-compile-run cycle

Some numbers from UX research:\
<100ms feels instant\
<300ms perceptible\
<1000ms feels like the machine is working\
\>1000ms leads to context switching

Changes in a single edit-compile-run cycle should be compiled in less than a second.
First-time builds and complete rebuils should still be fast.

https://www.youtube.com/watch?v=14zlJ98gJKA&t=1104s
The Jai compiler on 21 Dec, 2016 could compile 33,326 lines of code in 0.36 seconds.
Assuming this scales linearly, this is approx. 100k lines/sec.

\#todo: create some initial performance goals, which will be refined and subdivided in the future
(e.g. lexing, parsing, typechecking, compilation, ...).
