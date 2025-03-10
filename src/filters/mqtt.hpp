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

#ifndef MQTT_HPP
#define MQTT_HPP

#include "filter.hpp"
#include "data.hpp"
#include "api/mqtt.hpp"
#include "options.hpp"

namespace pipy {
namespace mqtt {

//
// DecoderFunction
//

class DecoderFunction : public EventFunction {
public:
  void reset();

private:
  virtual void on_event(Event *evt) override;

  enum State {
    FIXED_HEADER,
    REMAINING_LENGTH,
    REMAINING_DATA,
    ERROR,
  };

  State m_state;
  uint8_t m_fixed_header;
  uint32_t m_remaining_length;
  int m_remaining_length_shift;
  int m_protocol_level;
  Data m_buffer;

  void message();

  virtual auto on_get_protocol_level() -> int { return 4; }
};

//
// EncoderFunction
//

class EncoderFunction : public EventFunction {
public:
  EncoderFunction();

  void reset();

private:
  virtual void on_event(Event *evt) override;

  pjs::Ref<MessageStart> m_start;
  Data m_buffer;
  pjs::PropertyCache m_prop_type;
  pjs::PropertyCache m_prop_qos;
  pjs::PropertyCache m_prop_dup;
  pjs::PropertyCache m_prop_retain;
  int m_protocol_level;

  virtual void on_encode_error(const char *msg) {}
};

//
// Decoder
//

class Decoder :
  public Filter,
  protected DecoderFunction
{
public:
  struct Options : public pipy::Options {
    int protocol_level = 4;
    pjs::Ref<pjs::Function> protocol_level_f;

    Options() {}
    Options(pjs::Object *options);
  };

  Decoder(const Options &options);

private:
  Decoder(const Decoder &r);
  ~Decoder();

  virtual auto clone() -> Filter* override;
  virtual void chain() override;
  virtual void reset() override;
  virtual void process(Event *evt) override;
  virtual void dump(Dump &d) override;

  virtual auto on_get_protocol_level() -> int override;

  Options m_options;
};

//
// Encoder
//

class Encoder :
  public Filter,
  protected EncoderFunction
{
public:
  Encoder();

private:
  Encoder(const Encoder &r);
  ~Encoder();

  virtual auto clone() -> Filter* override;
  virtual void chain() override;
  virtual void reset() override;
  virtual void process(Event *evt) override;
  virtual void dump(Dump &d) override;

  virtual void on_encode_error(const char *msg) override;
};

} // namespace mqtt
} // namespace pipy

#endif // MQTT_HPP
