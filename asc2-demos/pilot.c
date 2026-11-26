/*
    Pilot: the burkut of the game flies by itself.

    The game is `burkut.c`, and a neuron in the chip plays it. In every
    frame the neuron gets four numbers: how far the burkut is above or below
    the middle of the next hole, how fast it rises or sinks, a constant, and
    how far the next pillar is. It multiplies each by a weight and adds the
    products (`dotv`); a sum above zero is a beat of the wings.

    The weights start at zero, so the first burkut does nothing and falls.
    Every crash is a lesson. After a crash below the hole or on the ground,
    the frames before it in which the burkut did not beat its wings were
    wrong, and the weights move so that it would (`learn`); after a crash
    above the hole, the frames in which it did were wrong. Nobody tells the
    neuron the rule of the game. After about twenty crashes it flies. The
    flights in which it learns run fast (">>" beside the number of the
    flight); once a flight has passed three pillars, it is shown at the
    speed of the game.

    A press of the button is a beat of the wings: a person takes the flight
    over until it ends. The button held down for two seconds ends the
    program.
*/

#define PILOT

#include "burkut.c"
