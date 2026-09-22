#include "platform/emscripten/ui.h"
#include "bitmap.h"
#include "output.h"
#include "player.h"
#include <algorithm>
#include <emscripten.h>
#include <sstream>

WebUi::WebAudio::WebAudio(const Game_ConfigAudio& cfg) : GenericAudio(cfg) {
	SetFormat(EM_ASM_INT({ return Module.sampleRate; }), AudioDecoder::Format::S16, 2);
}

WebUi::WebUi(int width, int height, const Game_Config& cfg) : BaseUi(cfg), audio(cfg.audio) {
	Bitmap::SetFormat(DynamicFormat(32, 8, 16, 8, 8, 8, 0, 8, 24, PF::Alpha));
	vChangeDisplaySurfaceResolution(width, height);
	SetFrameRateSynchronized(true);
	// Publish the engine's key names instead of duplicating its enum in JavaScript.
	for (int i = 1; i < Input::Keys::KEYS_COUNT; ++i) {
		const auto name = Input::Keys::kInputKeyNames[static_cast<Input::Keys::InputKey>(i)];
		const auto key = std::string(name);
		EM_ASM({ Module.keyNames[UTF8ToString($0)] = $1; }, key.c_str(), i);
	}
}

bool WebUi::vChangeDisplaySurfaceResolution(int width, int height) {
	main_surface = Bitmap::Create(width, height, false, 32);
	current_display_mode.width = width;
	current_display_mode.height = height;
	return bool(main_surface);
}

bool WebUi::ProcessEvents() { return true; }

void WebUi::UpdateDisplay() {
	EM_ASM({ Module.present($0, $1, $2, $3); }, main_surface->pixels(),
		main_surface->width(), main_surface->height(), main_surface->pitch());
}

void WebUi::vGetConfig(Game_ConfigVideo& cfg) const {
	cfg.fps.SetOptionVisible(true);
	cfg.game_resolution.SetOptionVisible(true);
	cfg.pause_when_focus_lost.SetOptionVisible(true);
}

void WebUi::ToggleFullscreen() { EM_ASM({ postMessage({type: 'fullscreen'}); }); }

bool WebUi::ShowCursor(bool visible) {
	const bool previous = cursor_visible;
	cursor_visible = visible;
	EM_ASM({ postMessage({type: 'cursor', visible: !!$0}); }, visible);
	return previous;
}

bool WebUi::OpenURL(std::string_view url) {
	const auto value = std::string(url);
	EM_ASM({ postMessage({type: 'url', url: UTF8ToString($0)}); }, value.c_str());
	return true;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void web_key(int key, int pressed) {
	if (DisplayUi && key > 0 && key < Input::Keys::KEYS_COUNT)
		DisplayUi->GetKeyStates().set(key, pressed != 0);
}

EMSCRIPTEN_KEEPALIVE void web_mouse(int x, int y, int focus) {
	if (DisplayUi) static_cast<WebUi&>(*DisplayUi).Mouse(x, y, focus != 0);
}

EMSCRIPTEN_KEEPALIVE void web_gamepad(float x, float y, float rx, float ry, float lt, float rt) {
	if (DisplayUi) static_cast<WebUi&>(*DisplayUi).Gamepad(x, y, rx, ry, lt, rt);
}

EMSCRIPTEN_KEEPALIVE void web_touch(int id, int x, int y, int pressed) {
	if (!DisplayUi) return;
	auto& fingers = DisplayUi->GetTouchInput();
	const auto finger = std::find_if(fingers.begin(), fingers.end(), [&](const auto& value) {
		return value.id == id || (pressed && value.id == -1);
	});
	if (finger == fingers.end()) return;
	if (pressed) finger->Down(id, x, y);
	else finger->Up();
}

EMSCRIPTEN_KEEPALIVE void web_focus(int focused, int fullscreen) {
	if (!DisplayUi) return;
	static_cast<WebUi&>(*DisplayUi).Fullscreen(fullscreen != 0);
	static int previous_focus = -1;
	if (previous_focus == focused) return;
	previous_focus = focused;
	if (!focused) {
		DisplayUi->GetKeyStates().reset();
		for (auto& finger : DisplayUi->GetTouchInput()) finger.Up();
		static_cast<WebUi&>(*DisplayUi).Gamepad(0, 0, 0, 0, 0, 0);
	}
	if (DisplayUi->GetConfig().pause_when_focus_lost.Get()) {
		EM_ASM({ Module.paused = !$0; }, focused);
		if (focused) Player::Resume();
		else Player::Pause();
	}
}

EMSCRIPTEN_KEEPALIVE void web_audio(uint8_t* buffer, int frames) {
	if (DisplayUi && frames > 0 && frames <= 4096)
		static_cast<GenericAudio&>(DisplayUi->GetAudio()).Decode(buffer, frames * 4);
}

EMSCRIPTEN_KEEPALIVE void web_capture() {
	if (!DisplayUi) return;
	std::ostringstream stream;
	Output::TakeScreenshot(stream);
	const auto png = stream.str();
	EM_ASM({
		const bytes = HEAPU8.slice($0, $0 + $1);
		postMessage({type: 'screenshot', bytes, width: $2, height: $3}, [bytes.buffer]);
	}, png.data(), png.size(), DisplayUi->GetWidth(), DisplayUi->GetHeight());
}

EMSCRIPTEN_KEEPALIVE void web_stop() {
	EM_ASM({ Module.paused = false; });
	Player::exit_flag = true;
}
}
