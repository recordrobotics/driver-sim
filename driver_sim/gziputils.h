#pragma once

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <string>

class GzipOutputBuf final : public std::streambuf
{
  public:
    explicit GzipOutputBuf(std::ostream &destination) : destination(destination)
    {
        stream = {};
        const int rc = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                    MAX_WBITS + 16, // gzip framing
                                    8, Z_DEFAULT_STRATEGY);

        if (rc != Z_OK)
        {
            throw std::runtime_error("deflateInit2 failed");
        }

        initialized = true;
        setp(input.data(), input.data() + input.size());
    }

    ~GzipOutputBuf() override
    {
        if (initialized)
        {
            deflateEnd(&stream);
        }
    }

    void finish()
    {
        if (finished)
        {
            return;
        }

        if (failed)
        {
            throw std::runtime_error("Gzip output stream previously failed");
        }

        flushInput();

        int rc;
        do
        {
            stream.next_in = nullptr;
            stream.avail_in = 0;
            stream.next_out = reinterpret_cast<Bytef *>(output.data());
            stream.avail_out = static_cast<uInt>(output.size());

            rc = deflate(&stream, Z_FINISH);

            if (rc != Z_OK && rc != Z_STREAM_END)
            {
                fail("deflate finish failed");
            }

            writeOutput(output.size() - stream.avail_out);
        } while (rc != Z_STREAM_END);

        if (!destination)
        {
            fail("Destination write failed");
        }

        finished = true;
    }

  protected:
    int_type overflow(int_type ch) override
    {
        if (finished || failed || flushInput() != 0)
        {
            return traits_type::eof();
        }

        if (!traits_type::eq_int_type(ch, traits_type::eof()))
        {
            *pptr() = traits_type::to_char_type(ch);
            pbump(1);
        }

        return traits_type::not_eof(ch);
    }

    std::streamsize xsputn(const char *data, std::streamsize size) override
    {
        std::streamsize total = 0;

        while (total < size && !failed && !finished)
        {
            if (pptr() == epptr() && flushInput() != 0)
            {
                break;
            }

            const auto available = epptr() - pptr();
            const auto count = std::min<std::streamsize>(size - total, available);

            std::memcpy(pptr(), data + total, static_cast<std::size_t>(count));

            pbump(static_cast<int>(count));
            total += count;
        }

        return total;
    }

    int sync() override
    {
        if (finished || failed || flushInput() != 0)
        {
            return -1;
        }

        int rc;
        do
        {
            stream.next_in = nullptr;
            stream.avail_in = 0;
            stream.next_out = reinterpret_cast<Bytef *>(output.data());
            stream.avail_out = static_cast<uInt>(output.size());

            rc = deflate(&stream, Z_SYNC_FLUSH);

            if (rc != Z_OK)
            {
                return fail("deflate sync flush failed");
            }

            writeOutput(output.size() - stream.avail_out);
        } while (stream.avail_out == 0);

        return destination ? 0 : fail("Destination write failed");
    }

  private:
    int flushInput()
    {
        const auto size = static_cast<std::size_t>(pptr() - pbase());
        std::size_t offset = 0;

        while (offset < size)
        {
            const auto chunk = static_cast<uInt>(
                std::min<std::size_t>(size - offset, std::numeric_limits<uInt>::max()));

            stream.next_in = reinterpret_cast<Bytef *>(pbase() + offset);
            stream.avail_in = chunk;

            while (stream.avail_in > 0)
            {
                stream.next_out = reinterpret_cast<Bytef *>(output.data());
                stream.avail_out = static_cast<uInt>(output.size());

                const int rc = deflate(&stream, Z_NO_FLUSH);
                if (rc != Z_OK)
                {
                    return fail("deflate failed");
                }

                writeOutput(output.size() - stream.avail_out);
            }

            offset += chunk;
        }

        setp(input.data(), input.data() + input.size());
        return failed ? -1 : 0;
    }

    void writeOutput(std::size_t size)
    {
        if (size == 0 || failed)
        {
            return;
        }

        destination.write(output.data(), static_cast<std::streamsize>(size));

        if (!destination)
        {
            failed = true;
        }
    }

    int fail(const char *message)
    {
        failed = true;
        throw std::runtime_error(message);
    }

    std::ostream &destination;
    z_stream stream{};
    std::array<char, 64 * 1024> input{};
    std::array<char, 64 * 1024> output{};
    bool initialized = false;
    bool finished = false;
    bool failed = false;
};

class GzipInputBuf final : public std::streambuf
{
  public:
    explicit GzipInputBuf(std::istream &source) : source(source)
    {
        stream = {};

        const int rc = inflateInit2(&stream, MAX_WBITS + 16);
        if (rc != Z_OK)
        {
            throw std::runtime_error("inflateInit2 failed");
        }

        initialized = true;
        setg(output.data(), output.data(), output.data());
    }

    ~GzipInputBuf() override
    {
        if (initialized)
        {
            inflateEnd(&stream);
        }
    }

  protected:
    int_type underflow() override
    {
        if (gptr() < egptr())
        {
            return traits_type::to_int_type(*gptr());
        }

        if (finished)
        {
            return traits_type::eof();
        }

        stream.next_out = reinterpret_cast<Bytef *>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        while (stream.avail_out > 0 && !finished)
        {
            if (stream.avail_in == 0)
            {
                source.read(input.data(), static_cast<std::streamsize>(input.size()));

                const auto count = source.gcount();

                if (count <= 0)
                {
                    throw std::runtime_error("Truncated gzip stream or input read failure");
                }

                stream.next_in = reinterpret_cast<Bytef *>(input.data());
                stream.avail_in = static_cast<uInt>(count);
            }

            const uInt beforeIn = stream.avail_in;
            const uInt beforeOut = stream.avail_out;

            const int rc = inflate(&stream, Z_NO_FLUSH);

            if (rc == Z_STREAM_END)
            {
                finished = true;
            }
            else if (rc != Z_OK)
            {
                throw std::runtime_error(
                    std::string("Gzip decompression failed: ") +
                    (stream.msg != nullptr ? stream.msg : "invalid gzip data"));
            }

            if (rc == Z_OK && beforeIn == stream.avail_in && beforeOut == stream.avail_out)
            {
                throw std::runtime_error("Gzip decompressor made no progress");
            }
        }

        const auto produced = output.size() - stream.avail_out;

        if (produced == 0)
        {
            return traits_type::eof();
        }

        setg(output.data(), output.data(), output.data() + produced);

        return traits_type::to_int_type(*gptr());
    }

  private:
    std::istream &source;
    z_stream stream{};
    std::array<char, 64 * 1024> input{};
    std::array<char, 64 * 1024> output{};
    bool initialized = false;
    bool finished = false;
};