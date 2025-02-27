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

#include "pipy.hpp"
#include "codebase.hpp"
#include "configuration.hpp"
#include "context.hpp"
#include "worker.hpp"
#include "worker-thread.hpp"
#include "net.hpp"
#include "utils.hpp"

namespace pipy {

static std::function<void(int)> s_on_exit;

void Pipy::on_exit(const std::function<void(int)> &on_exit) {
  s_on_exit = on_exit;
}

void Pipy::operator()(pjs::Context &ctx, pjs::Object *obj, pjs::Value &ret) {
  pjs::Value ret_obj;
  pjs::Object *context_prototype = nullptr;
  if (!ctx.arguments(0, &context_prototype)) return;
  if (context_prototype && context_prototype->is_function()) {
    auto *f = context_prototype->as<pjs::Function>();
    (*f)(ctx, 0, nullptr, ret_obj);
    if (!ctx.ok()) return;
    if (!ret_obj.is_object()) {
      ctx.error("function did not return an object");
      return;
    }
    context_prototype = ret_obj.o();
  }
  try {
    auto config = Configuration::make(context_prototype);
    ret.set(config);
  } catch (std::runtime_error &err) {
    ctx.error(err);
  }
}

} // namespace pipy

namespace pjs {

using namespace pipy;

template<> void ClassDef<Pipy>::init() {
  super<Function>();
  ctor();

  method("load", [](Context &ctx, Object*, Value &ret) {
    std::string filename;
    if (!ctx.arguments(1, &filename)) return;
    auto path = utils::path_normalize(filename);
    auto data = Codebase::current()->get(path);
    ret.set(data ? pipy::Data::make(*data) : nullptr);
    if (data) data->release();
  });

  method("solve", [](Context &ctx, Object*, Value &ret) {
    Str *filename;
    if (!ctx.arguments(1, &filename)) return;
    auto worker = static_cast<pipy::Context*>(ctx.root())->worker();
    worker->solve(ctx, filename, ret);
  });

  method("restart", [](Context&, Object*, Value&) {
    Net::main().post(
      []() {
        InputContext ic;
        Codebase::current()->sync(
          true, [](bool ok) {
            if (ok) {
              WorkerManager::get().reload();
            }
          }
        );
      }
    );
  });

  method("exit", [](Context &ctx, Object*, Value&) {
    int exit_code = 0;
    if (!ctx.arguments(0, &exit_code)) return;
    Net::main().post(
      [=]() {
        WorkerManager::get().stop(true);
        if (s_on_exit) s_on_exit(exit_code);
      }
    );
  });
}

} // namespace pjs
