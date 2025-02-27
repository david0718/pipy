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

#ifndef CONNECT_HPP
#define CONNECT_HPP

#include "filter.hpp"
#include "outbound.hpp"
#include "options.hpp"

namespace pipy {

//
// ConnectReceiver
//

class ConnectReceiver : public EventTarget {
  virtual void on_event(Event *evt) override;
};

//
// Connect
//

class Connect : public Filter, public ConnectReceiver {
public:
  struct Options : public pipy::Options, public Outbound::Options {
    pjs::Ref<pjs::Str> bind;
    pjs::Ref<pjs::Function> bind_f;
    Options() {}
    Options(const Outbound::Options &options) : Outbound::Options(options) {}
    Options(pjs::Object *options);
  };

  Connect(const pjs::Value &target, const Options &options);

private:
  Connect(const Connect &r);
  ~Connect();

  virtual auto clone() -> Filter* override;
  virtual void reset() override;
  virtual void process(Event *evt) override;
  virtual void dump(Dump &d) override;

  pjs::Value m_target;
  pjs::Ref<Outbound> m_outbound;
  Options m_options;

  friend class ConnectReceiver;
};

} // namespace pipy

#endif // CONNECT_HPP
