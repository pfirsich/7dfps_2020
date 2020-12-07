#include "serialization.hpp"

struct A {
    int y;

    SERIALIZE()
    {
        FIELD(y);
        SERIALIZE_END;
    }
};

struct B {
    uint8_t c;
    uint32_t u;
    int x;
    float f;
    glm::vec3 v;
    A a;
    std::vector<A> as;
    std::vector<int> is;

    SERIALIZE()
    {
        FIELD(c);
        FIELD(u);
        FIELD(x);
        FIELD(f);
        FIELD(v);
        FIELD(a);
        FIELD_VEC(as);
        FIELD_VEC(is);
        SERIALIZE_END;
    }
};

struct ClientMoveUpdate {
    uint8_t inputs; // bitmask
    glm::quat orientation;

    SERIALIZE()
    {
        FIELD(inputs);
        FIELD(orientation);
        SERIALIZE_END;
    }
};

struct ServerPlayerStateUpdate {
    struct PlayerState {
        uint32_t guid;
        glm::vec3 position;
        glm::quat orientation;

        SERIALIZE()
        {
            FIELD(guid);
            FIELD(position);
            FIELD(orientation);
            SERIALIZE_END;
        }
    };

    std::vector<PlayerState> players;

    SERIALIZE()
    {
        FIELD_VEC(players);
        SERIALIZE_END;
    }
};

int main(int, char**)
{
    WriteBuffer wbuf(1024);
    B src { 69, 0x12345678, -589589, 89.484f, glm::vec3(3.0, 4.0f, 2.0), A { 12 },
        { A { 59 }, A { 68 }, A { 92 }, A { 39 } }, { 5, 753, 8493, 8, 482948, 999 } };
    if (!serialize(wbuf, src))
        fmt::print(stderr, "Error serializing\n");

    fmt::print("data: ");
    for (size_t i = 0; i < wbuf.getSize(); ++i)
        fmt::print("{:02x} ", wbuf.getData()[i]);
    fmt::print("\n");

    ReadBuffer rbuf(wbuf.getData(), wbuf.getSize());
    B dst;
    if (!deserialize(rbuf, dst)) {
        fmt::print(stderr, "Error deserializing\n");
    } else {
        fmt::print("B = {{c = {}, u = {:x}, x = {}, f = {}, v = {}, a = {{y = {}}} }}\n", dst.c,
            dst.u, dst.x, dst.f, dst.v, dst.a.y);
        for (size_t i = 0; i < dst.as.size(); ++i)
            fmt::print("B.as[{}].y = {}\n", i, dst.as[i].y);
        for (size_t i = 0; i < dst.is.size(); ++i)
            fmt::print("B.is[{}].y = {}\n", i, dst.is[i]);
    }

    ServerPlayerStateUpdate psu;
    wbuf.clear();
    if (!serialize(wbuf, psu))
        fmt::print(stderr, "Error serializing psu\n");
    ReadBuffer rbuf2(wbuf.getData(), wbuf.getSize());
    if (!deserialize(rbuf2, psu))
        fmt::print(stderr, "Error deserializing\n");
    return 0;
}
