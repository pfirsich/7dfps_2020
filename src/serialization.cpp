#include "serialization.hpp"

#include <cstring>
#include <string>
#include <vector>

using StringLength = uint32_t;
static constexpr auto MaxStringLength = std::numeric_limits<StringLength>::max();

#ifndef _WIN32
float ntohf(uint32_t val)
{
    static_assert(sizeof(float) == sizeof(uint32_t));
    const uint32_t i = ntoh(val);
    float f = 0.0f;
    std::memcpy(&f, &i, sizeof(uint32_t));
    return f;
}

uint32_t htonf(float val)
{
    static_assert(sizeof(float) == sizeof(uint32_t));
    uint32_t i = 0;
    std::memcpy(&i, &val, sizeof(float));
    return hton(i);
}
#endif

WriteBuffer::WriteBuffer(size_t capacity)
{
    data_.reserve(capacity);
}

void WriteBuffer::reserve(size_t capacity)
{
    data_.reserve(capacity);
}

void WriteBuffer::fit(size_t numBytes)
{
    data_.reserve(data_.size() + numBytes);
}

const uint8_t* WriteBuffer::getData() const
{
    return data_.data();
}

size_t WriteBuffer::getSize() const
{
    return data_.size();
}

size_t WriteBuffer::getCapacity() const
{
    return data_.capacity();
}

void WriteBuffer::clear()
{
    return data_.clear();
}

WriteStream::WriteStream(WriteBuffer& buffer)
    : buffer_(buffer)
{
}

bool WriteStream::serialize(bool v)
{
    return serializeInt<uint8_t>(v ? 1 : 0);
}

bool WriteStream::serialize(uint8_t v)
{
    return serializeInt(v);
}

bool WriteStream::serialize(int8_t v)
{
    return serializeInt(v);
}

bool WriteStream::serialize(uint16_t v)
{
    return serializeInt(v);
}

bool WriteStream::serialize(int16_t v)
{
    return serializeInt(v);
}

bool WriteStream::serialize(uint32_t v)
{
    return serializeInt(v);
}

bool WriteStream::serialize(int32_t v)
{
    return serializeInt(v);
}

bool WriteStream::serialize(float val)
{
    buffer_.write(htonf(val));
    return true;
}

bool WriteStream::serialize(std::string& str)
{
    assert(str.size() <= MaxStringLength);
    if (!serialize(static_cast<StringLength>(str.size())))
        return false;
    buffer_.write(str.data(), str.size());
    return true;
}

bool WriteStream::serialize(glm::vec2& v)
{
    return serializeFor(v, 2);
}

bool WriteStream::serialize(glm::vec3& v)
{
    return serializeFor(v, 3);
}

bool WriteStream::serialize(glm::vec4& v)
{
    return serializeFor(v, 4);
}

bool WriteStream::serialize(glm::quat& q)
{
    return serializeFor(q, 4);
}

ReadBuffer::ReadBuffer(const uint8_t* data, size_t size)
    : data_(data)
    , size_(size)
{
}

size_t ReadBuffer::getCursor() const
{
    return cursor_;
}

size_t ReadBuffer::getLeft() const
{
    return size_ - cursor_;
}

bool ReadBuffer::canRead(size_t numBytes) const
{
    return getLeft() >= numBytes;
}

ReadStream::ReadStream(ReadBuffer& buffer)
    : buffer_(buffer)
{
}

bool ReadStream::serialize(bool& v)
{
    uint8_t i = 0;
    const auto ret = serializeInt(i);
    v = i != 0;
    return ret;
}

bool ReadStream::serialize(uint8_t& v)
{
    return serializeInt(v);
}

bool ReadStream::serialize(int8_t& v)
{
    return serializeInt(v);
}

bool ReadStream::serialize(uint16_t& v)
{
    return serializeInt(v);
}

bool ReadStream::serialize(int16_t& v)
{
    return serializeInt(v);
}

bool ReadStream::serialize(uint32_t& v)
{
    return serializeInt(v);
}

bool ReadStream::serialize(int32_t& v)
{
    return serializeInt(v);
}

bool ReadStream::serialize(float& val)
{
    uint32_t r;
    if (!buffer_.read(r))
        return false;
    val = ntohf(r);
    return true;
}

bool ReadStream::serialize(std::string& str)
{
    StringLength size = 0;
    if (!serialize(size))
        return false;
    str.resize(size, 0);
    return buffer_.read(str.data(), size);
}

bool ReadStream::serialize(glm::vec2& v)
{
    return serializeFor(v, 2);
}

bool ReadStream::serialize(glm::vec3& v)
{
    return serializeFor(v, 3);
}

bool ReadStream::serialize(glm::vec4& v)
{
    return serializeFor(v, 4);
}

bool ReadStream::serialize(glm::quat& q)
{
    return serializeFor(q, 4);
}
