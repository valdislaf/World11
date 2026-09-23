#include "hg_game_app.hpp"

#include <stdexcept>
#include <utility>

namespace hg {

GameApp::GameApp(std::unique_ptr<IInput> input,
                 std::unique_ptr<IWorld> world,
                 std::unique_ptr<IRenderer> renderer)
    : input_(std::move(input)), world_(std::move(world)), renderer_(std::move(renderer)) {
  if (!input_ || !world_ || !renderer_) {
    throw std::invalid_argument("GameApp requires input/world/renderer");
  }
}

void GameApp::runUntilQuit(double dt) {
  while (true) {
    input_->poll();
    if (input_->shouldQuit()) {
      break;
    }

    world_->tick(engine_, dt);
    renderer_->render(engine_, *world_);
  }
}

const Engine& GameApp::engine() const {
  return engine_;
}

}  // namespace hg
