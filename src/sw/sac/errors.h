/**************************************************************************
   Copyright (c) 2026 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#ifndef SEWENEW_SAC_ERRORS_H
#define SEWENEW_SAC_ERRORS_H

#include <stdexcept>
#include <string>

namespace sw::sac {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string &msg) : std::runtime_error(msg) {}

    Error(const Error &) = default;
    Error &operator=(const Error &) = default;
    Error(Error &&) = default;
    Error &operator=(Error &&) = default;
    virtual ~Error() override = default;
};

class LlmError : public Error {
public:
    explicit LlmError(const std::string &msg) : Error(msg) {}

    LlmError(const LlmError &) = default;
    LlmError &operator=(const LlmError &) = default;
    LlmError(LlmError &&) = default;
    LlmError &operator=(LlmError &&) = default;
    virtual ~LlmError() override = default;
};

// Non-2xx HTTP status from the transport layer.
class HttpError : public Error {
public:
    HttpError(long status_code, const std::string &msg)
            : Error(msg), _status_code(status_code) {}

    HttpError(const HttpError &) = default;
    HttpError &operator=(const HttpError &) = default;
    HttpError(HttpError &&) = default;
    HttpError &operator=(HttpError &&) = default;
    virtual ~HttpError() override = default;

    long status_code() const noexcept { return _status_code; }

private:
    long _status_code;
};

// Provider returned well-formed HTTP but signalled an error in the JSON body.
class ApiError : public Error {
public:
    explicit ApiError(const std::string &msg) : Error(msg) {}

    ApiError(const ApiError &) = default;
    ApiError &operator=(const ApiError &) = default;
    ApiError(ApiError &&) = default;
    ApiError &operator=(ApiError &&) = default;
    virtual ~ApiError() override = default;
};

// Response body could not be parsed as expected JSON.
class ParseError : public Error {
public:
    explicit ParseError(const std::string &msg) : Error(msg) {}

    ParseError(const ParseError &) = default;
    ParseError &operator=(const ParseError &) = default;
    ParseError(ParseError &&) = default;
    ParseError &operator=(ParseError &&) = default;
    virtual ~ParseError() override = default;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_ERRORS_H
