#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <enet/enet.h>

enum class StreamType { Read, Write };

template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
T ntoh(T val)
{
    using U = std::make_unsigned_t<T>;
    static_assert(
        std::is_same_v<U, uint8_t> || std::is_same_v<U, uint16_t> || std::is_same_v<U, uint32_t>,
        "Invalid type");
    if constexpr (std::is_same_v<U, uint8_t>) {
        return val;
    } else if constexpr (std::is_same_v<U, uint16_t>) {
        return ntohs(val);
    } else if constexpr (std::is_same_v<U, uint32_t>) {
        return ntohl(val);
    }
    // This should never happen
    return val;
}

template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
T hton(T val)
{
    using U = std::make_unsigned_t<T>;
    static_assert(
        std::is_same_v<U, uint8_t> || std::is_same_v<U, uint16_t> || std::is_same_v<U, uint32_t>,
        "Invalid type");
    if constexpr (std::is_same_v<U, uint8_t>) {
        return val;
    } else if constexpr (std::is_same_v<U, uint16_t>) {
        return htons(val);
    } else if constexpr (std::is_same_v<U, uint32_t>) {
        return htonl(val);
    }
    // This should never happen
    return val;
}

#ifndef _WIN32
float ntohf(uint32_t val);
uint32_t htonf(float val);
#endif

class WriteBuffer {
public:
    WriteBuffer(size_t capacity);

    void reserve(size_t capacity);

    // Makes sure there is enough capacity for numBytes additional bytes
    void fit(size_t numBytes);

    template <typename T>
    void write(const T* ptr, size_t num)
    {
        const auto data = reinterpret_cast<const uint8_t*>(ptr);
        data_.insert(data_.end(), data, data + sizeof(T) * num);
    }

    template <typename T>
    void write(const T& obj)
    {
        write(&obj, 1);
    }

    const uint8_t* getData() const;
    size_t getSize() const;
    size_t getCapacity() const;

    void clear();

private:
    std::vector<uint8_t> data_;
};

class WriteStream {
public:
    static constexpr StreamType Type = StreamType::Write;

    WriteStream(WriteBuffer& buffer);

    template <typename T>
    bool serialize(T& obj)
    {
        return obj.serialize(*this);
    }

    bool serialize(uint8_t v);
    bool serialize(int8_t v);
    bool serialize(uint16_t v);
    bool serialize(int16_t v);
    bool serialize(uint32_t v);
    bool serialize(int32_t v);
    bool serialize(float val);
    bool serialize(std::string& str);
    bool serialize(glm::vec2& v);
    bool serialize(glm::vec3& v);
    bool serialize(glm::vec4& v);
    bool serialize(glm::quat& q);

    // No partial function template specialization :(
    template <typename T>
    bool serializeVector(std::vector<T>& vec)
    {
        assert(vec.size() <= std::numeric_limits<uint8_t>::max());
        if (!serialize(static_cast<uint8_t>(vec.size())))
            return false;
        for (auto& v : vec)
            if (!serialize(v))
                return false;
        return true;
    }

private:
    template <typename T>
    bool serializeInt(T val)
    {
        static_assert(std::is_integral_v<T>);
        buffer_.write(hton(val));
        return true;
    }

    template <typename T>
    bool serializeFor(T& c, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            buffer_.write(c[i]);
        return true;
    }

    WriteBuffer& buffer_;
};

class ReadBuffer {
public:
    ReadBuffer(const uint8_t* data, size_t size);

    template <typename T>
    bool read(T* ptr, size_t num)
    {
        const auto numBytes = sizeof(T) * num;
        if (!canRead(numBytes))
            return false;
        std::memcpy(ptr, data_ + cursor_, numBytes);
        cursor_ += numBytes;
        return true;
    }

    template <typename T>
    bool read(T& obj)
    {
        return read(&obj, 1);
    }

    size_t getCursor() const;
    size_t getLeft() const;
    bool canRead(size_t numBytes) const;

private:
    const uint8_t* data_;
    size_t size_;
    size_t cursor_ = 0;
};

class ReadStream {
public:
    static constexpr StreamType Type = StreamType::Write;

    ReadStream(ReadBuffer& buffer);

    template <typename T>
    bool serialize(T& obj)
    {
        return obj.serialize(*this);
    }

    bool serialize(uint8_t& v);
    bool serialize(int8_t& v);
    bool serialize(uint16_t& v);
    bool serialize(int16_t& v);
    bool serialize(uint32_t& v);
    bool serialize(int32_t& v);
    bool serialize(float& val);
    bool serialize(std::string& str);
    bool serialize(glm::vec2& v);
    bool serialize(glm::vec3& v);
    bool serialize(glm::vec4& v);
    bool serialize(glm::quat& q);

    // No partial function template specialization :(
    template <typename T>
    bool serializeVector(std::vector<T>& vec)
    {
        uint8_t num;
        if (!serialize(num))
            return false;
        vec.resize(num);
        for (size_t i = 0; i < num; ++i)
            if (!serialize(vec[i]))
                return false;
        return true;
    }

private:
    template <typename T>
    bool serializeInt(T& val)
    {
        static_assert(std::is_integral_v<T>);
        T r;
        if (!buffer_.read(r))
            return false;
        val = ntoh(r);
        return true;
    }

    template <typename T>
    bool serializeFor(T& c, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            if (!buffer_.read(c[i]))
                return false;
        return true;
    }

    ReadBuffer& buffer_;
};

#define SERIALIZE()                                                                                \
    template <typename Stream>                                                                     \
    bool serialize(Stream& stream)

#define SERIALIZE_END return true

#define SERIALIZING (Stream::Type == StreamType::Write)
#define DESERIALIZING (Stream::Type == StreamType::Read)

#define FIELD(obj)                                                                                 \
    do {                                                                                           \
        if (!stream.serialize(obj))                                                                \
            return false;                                                                          \
    } while (0)

#define FIELD_VEC(vec)                                                                             \
    do {                                                                                           \
        if (!stream.serializeVector(vec))                                                          \
            return false;                                                                          \
    } while (0)

template <typename T>
bool serialize(WriteBuffer& buffer, T& object)
{
    WriteStream stream(buffer);
    return stream.serialize(object);
}

template <typename T>
bool deserialize(ReadBuffer& buffer, T& object)
{
    ReadStream stream(buffer);
    return stream.serialize(object);
}
