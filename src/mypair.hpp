#pragma once

template <typename A, typename B> struct MyPair {
    A first;
    B second;

    MyPair() = default;
    MyPair(A first, B second) : first(first), second(second) {}
};

using IntPair = MyPair<int, int>;
