/*
 *  Copyright (c) 2019 by flomesh.io
 *
 *  Unless prior written consent has been obtained from the copyright
 *  owner, the following shall not be allowed.
 *
 *  1. The distribution of any source codes, header files, make files,
 *     or libraries of the software.
 *
 *  2. Disclosure of any source codes pertaining to the software to any
 *     additional parties.
 *
 *  3. Alteration or removal of any notices in or on the software or
 *     within the documentation included within the software.
 *
 *  ALL SOURCE CODE AS WELL AS ALL DOCUMENTATION INCLUDED WITH THIS
 *  SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION, WITHOUT WARRANTY OF ANY
 *  KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "connect.hpp"
#include "outbound.hpp"
#include "utils.hpp"
#include "log.hpp"

namespace pjs {

using namespace pipy;

template<>
void EnumDef<Outbound::Protocol>::init() {
  define(Outbound::Protocol::TCP, "tcp");
  define(Outbound::Protocol::UDP, "udp");
}

} // namespace pjs

namespace pipy {

//
// Connect::Options
//

Connect::Options::Options(pjs::Object *options) {
  Value(options, "protocol")
    .get_enum(protocol)
    .check_nullable();
  Value(options, "bind")
    .get(bind)
    .get(bind_f)
    .check_nullable();
  Value(options, "bufferLimit")
    .get_binary_size(buffer_limit)
    .check_nullable();
  Value(options, "retryCount")
    .get(retry_count)
    .check_nullable();
  Value(options, "retryDelay")
    .get_seconds(retry_delay)
    .check_nullable();
  Value(options, "connectTimeout")
    .get_seconds(connect_timeout)
    .check_nullable();
  Value(options, "readTimeout")
    .get_seconds(read_timeout)
    .check_nullable();
  Value(options, "writeTimeout")
    .get_seconds(write_timeout)
    .check_nullable();
  Value(options, "idleTimeout")
    .get_seconds(idle_timeout)
    .check_nullable();
  Value(options, "keepAlive")
    .get(keep_alive)
    .check_nullable();
}

//
// Connect
//

Connect::Connect(const pjs::Value &target, const Options &options)
  : m_target(target)
  , m_options(options)
{
}

Connect::Connect(const Connect &r)
  : Filter(r)
  , m_target(r.m_target)
  , m_options(r.m_options)
{
}

Connect::~Connect() {
}

void Connect::dump(Dump &d) {
  Filter::dump(d);
  d.name = "connect";
}

auto Connect::clone() -> Filter* {
  return new Connect(*this);
}

void Connect::reset() {
  Filter::reset();
  ConnectReceiver::close();
  if (m_outbound) {
    m_outbound->reset();
    m_outbound = nullptr;
  }
}

void Connect::process(Event *evt) {
  if (!m_outbound) {
    if (evt->is<StreamEnd>()) {
      Filter::output(evt);
      return;
    } else {
      pjs::Value target;
      if (!eval(m_target, target)) return;
      auto s = target.to_string();
      std::string host; int port;
      if (utils::get_host_port(s->str(), host, port)) {
        pjs::Ref<pjs::Str> bind(m_options.bind);
        if (m_options.bind_f) {
          pjs::Value ret;
          if (!Filter::eval(m_options.bind_f, ret)) return;
          if (!ret.is_string()) {
            Log::error("[connect] options.bind expected to return a string");
            return;
          }
          bind = ret.s();
        }

        Outbound *outbound = nullptr;
        switch (m_options.protocol) {
          case Outbound::Protocol::TCP:
            outbound = new OutboundTCP(ConnectReceiver::input(), m_options);
            break;
          case Outbound::Protocol::UDP:
            outbound = new OutboundUDP(ConnectReceiver::input(), m_options);
            break;
        }

        if (bind) {
          const auto &str = bind->str();
          std::string ip;
          int port;
          if (!utils::get_host_port(str, ip, port)) {
            ip = str;
            port = 0;
          }
          try {
            outbound->bind(ip, port);
          } catch (std::runtime_error &e) {
            Log::error("[connect] %s", e.what());
          }
        }

        outbound->connect(host, port);
        m_outbound = outbound;

      } else {
        Log::error("[connect] invalid target: %s", s->c_str());
      }
      s->release();
    }
  }

  if (m_outbound) {
    m_outbound->send(evt);
  }
}

//
// ConnectReceiver
//

void ConnectReceiver::on_event(Event *evt) {
  auto *conn = static_cast<Connect*>(this);
  conn->output(evt);
}

} // namespace pipy
