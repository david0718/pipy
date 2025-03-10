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

#include "main-options.hpp"
#include "fs.hpp"
#include "data.hpp"
#include "utils.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

namespace pipy {

thread_local static Data::Producer s_dp("Command Line Options");

void MainOptions::show_help() {
  std::cout << "Usage: pipy [options] [<filename or URL>]" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -h, -help, --help                    Show help information" << std::endl;
  std::cout << "  -v, -version, --version              Show version information" << std::endl;
  std::cout << "  -e, -eval, --eval                    Evaluate the given string as script" << std::endl;
  std::cout << "  --threads=<number>                   Number of worker threads (1, 2, ... max)" << std::endl;
  std::cout << "  --log-level=<debug|info|warn|error>  Set the level of log output" << std::endl;
  std::cout << "  --verify                             Verify configuration only" << std::endl;
  std::cout << "  --no-graph                           Do not print pipeline graphs to the log" << std::endl;
  std::cout << "  --instance-uuid=<uuid>               Specify a UUID for this worker process" << std::endl;
  std::cout << "  --instance-name=<name>               Specify a name for this worker process" << std::endl;
  std::cout << "  --reuse-port                         Enable kernel load balancing for all listening ports" << std::endl;
  std::cout << "  --admin-port=<[[ip]:]port>           Enable administration service on the specified port" << std::endl;
  std::cout << "  --admin-tls-cert=<filename>          Administration service certificate" << std::endl;
  std::cout << "  --admin-tls-key=<filename>           Administration service private key" << std::endl;
  std::cout << "  --admin-tls-trusted=<filename>       Client certificate(s) trusted by administration service" << std::endl;
  std::cout << "  --tls-cert=<filename>                Client certificate in communication to administration service" << std::endl;
  std::cout << "  --tls-key=<filename>                 Client private key in communication to administration service" << std::endl;
  std::cout << "  --tls-trusted=<filename>             Administration service certificate(s) trusted by client" << std::endl;
  std::cout << "  --openssl-engine=<id>                Select an OpenSSL engine for the ciphers" << std::endl;
  std::cout << std::endl;
}

static const struct {
  Log::Topic topic;
  const char *name;
} s_topic_names[] = {
  { Log::ALLOC    , "alloc" },
  { Log::DUMP     , "dump" },
  { Log::INBOUND  , "inbound" },
  { Log::OUTBOUND , "outbound" },
  { Log::FILES    , "files" },
  { Log::SUBPROC  , "subproc" },
  { Log::USER     , "user" },
  { Log::NO_TOPIC , nullptr },
};

MainOptions::MainOptions(int argc, char *argv[]) {
  auto max_threads = std::thread::hardware_concurrency();

  for (int i = 1; i < argc; i++) {
    std::string term(argv[i]);
    if (term[0] != '-') {
      if (!filename.empty()) {
        std::string msg("redundant filename: ");
        throw std::runtime_error(msg + term);
      }
      filename = term;
    } else {
      auto i = term.find('=');
      auto k = (i == std::string::npos ? term : term.substr(0, i));
      auto v = (i == std::string::npos ? std::string() : term.substr(i + 1));
      if (k == "-v" || k == "-version" || k == "--version") {
        version = true;
      } else if (k == "-h" || k == "-help" || k == "--help") {
        help = true;
      } else if (k == "-e" || k == "-eval" || k == "--eval") {
        eval = true;
      } else if (k == "--threads") {
        if (v == "max") {
          threads = max_threads;
        } else {
          char *end;
          threads = std::strtol(v.c_str(), &end, 10);
          if (*end) throw std::runtime_error("--threads expects a number");
          if (threads <= 0) throw std::runtime_error("invalid number of threads");
          if (threads > max_threads) {
            std::string msg("number of threads exceeds the maximum ");
            throw std::runtime_error(msg + std::to_string(max_threads));
          }
        }
      } else if (k == "--log-level") {
        if (
          utils::starts_with(v, "debug") && (
          v.length() == 5 ||
          v.at(5) == ':')
        ) {
          log_level = Log::DEBUG;
          if (v.length() == 5) {
            log_topics = 0xffffffff;
          } else {
            log_topics = 0;
            for (const auto &topic : utils::split(v.substr(6), '+')) {
              int mask = 0;
              for (int i = 0; s_topic_names[i].name; i++) {
                if (topic == s_topic_names[i].name) {
                  mask = s_topic_names[i].topic;
                  break;
                }
              }
              if (!mask) {
                std::string msg("unknown log topic: ");
                msg += topic;
                msg += " (available topics include: ";
                for (int i = 0; s_topic_names[i].name; i++) {
                  if (i > 0) msg += " | ";
                  msg += s_topic_names[i].name;
                }
                msg += ')';
                throw std::runtime_error(msg);
              }
              log_topics |= mask;
            }
          }
        }
        else if (v == "warn") log_level = Log::WARN;
        else if (v == "error") log_level = Log::ERROR;
        else if (v == "info") log_level = Log::INFO;
        else {
          std::string msg("unknown log level: ");
          throw std::runtime_error(msg + v);
        }
      } else if (k == "--verify") {
        verify = true;
      } else if (k == "--no-graph") {
        no_graph = true;
      } else if (k == "--instance-uuid") {
        instance_uuid = v;
      } else if (k == "--instance-name") {
        instance_name = v;
      } else if (k == "--reuse-port") {
        reuse_port = true;
      } else if (k == "--admin-port") {
        admin_port = v;
      } else if (k == "--admin-tls-cert") {
        admin_tls_cert = load_certificate(v);
      } else if (k == "--admin-tls-key") {
        admin_tls_key = load_private_key(v);
      } else if (k == "--admin-tls-trusted") {
        load_certificate_list(v, admin_tls_trusted);
      } else if (k == "--tls-cert") {
        tls_cert = load_certificate(v);
      } else if (k == "--tls-key") {
        tls_key = load_private_key(v);
      } else if (k == "--tls-trusted") {
        load_certificate_list(v, tls_trusted);
      } else if (k == "--openssl-engine") {
        openssl_engine = v;
      } else {
        std::string msg("unknown option: ");
        throw std::runtime_error(msg + k);
      }
    }
  }

  if (eval && filename.empty()) {
    throw std::runtime_error("missing script to evaluate");
  }

  if (!instance_uuid.empty() && instance_uuid.find('/') != std::string::npos) {
    throw std::runtime_error("--instance-uuid does not allow slashes");
  }

  if (!admin_port.empty()) {
    std::string host;
    int port;
    if (utils::get_host_port(admin_port, host, port)) {
      uint8_t ip[16];
      if (!host.empty() && !utils::get_ip_v4(host.c_str(), ip) && !utils::get_ip_v6(host.c_str(), ip)) {
        throw std::runtime_error("invalid --admin-port");
      }
    } else {
      char *ptr_end;
      if (std::strtol(admin_port.c_str(), &ptr_end, 10) <= 0 || *ptr_end) {
        throw std::runtime_error("invalid --admin-port");
      }
    }
  }

  if (bool(admin_tls_cert) != bool(admin_tls_key)) {
    throw std::runtime_error("--admin-tls-cert and --admin-tls-key must be used in conjunction");
  }

  if (!admin_tls_trusted.empty() && !admin_tls_cert) {
    throw std::runtime_error("--admin-tls-cert and --admin-tls-key are required for --admin-tls-trusted");
  }

  if (bool(tls_cert) != bool(tls_key)) {
    throw std::runtime_error("--tls-cert and --tls-key must be used in conjunction");
  }
}

auto MainOptions::load_private_key(const std::string &filename) -> crypto::PrivateKey* {
  std::vector<uint8_t> buf;
  if (!fs::read_file(filename, buf)) {
    std::string msg("cannot open file: ");
    throw std::runtime_error(msg + filename);
  }
  pjs::Ref<Data> data = s_dp.make(&buf[0], buf.size());
  return crypto::PrivateKey::make(data);
}

auto MainOptions::load_certificate(const std::string &filename) -> crypto::Certificate* {
  std::vector<uint8_t> buf;
  if (!fs::read_file(filename, buf)) {
    std::string msg("cannot open file: ");
    throw std::runtime_error(msg + filename);
  }
  pjs::Ref<Data> data = s_dp.make(&buf[0], buf.size());
  return crypto::Certificate::make(data);
}

void MainOptions::load_certificate_list(const std::string &filename, std::vector<pjs::Ref<crypto::Certificate>> &list) {
  if (fs::is_file(filename)) {
    list.push_back(load_certificate(filename));

  } else if (fs::is_dir(filename)) {
    std::list<std::string> names;
    if (!fs::read_dir(filename, names)) {
      std::string msg("cannot read directory: ");
      throw std::runtime_error(msg + filename);
    }
    for (const auto &name : names) {
      list.push_back(load_certificate(
        utils::path_join(filename, name)
      ));
    }
  } else {
    std::string msg("file or directory not found: ");
    throw std::runtime_error(msg + filename);
  }
}

} // namespace pipy
