#ifndef EP_WEB_UI_H
#define EP_WEB_UI_H

#include "baseui.h"
#include "audio_generic.h"

// The browser owns the window; the engine, file reads and mixer share one Worker.
class WebUi final : public BaseUi {
public:
	WebUi(int width, int height, const Game_Config& cfg);
	bool ProcessEvents() override;
	void UpdateDisplay() override;
	bool vChangeDisplaySurfaceResolution(int width, int height) override;
	void vGetConfig(Game_ConfigVideo& cfg) const override;
	void ToggleFullscreen() override;
	bool ShowCursor(bool visible) override;
	bool OpenURL(std::string_view url) override;
	AudioInterface& GetAudio() override { return audio; }
	void Mouse(int x, int y, bool focus) { mouse_pos = {x, y}; mouse_focus = focus; }
	void Fullscreen(bool value) { SetIsFullscreen(value); }
	void Gamepad(float x, float y, float rx, float ry, float lt, float rt) {
		analog_input.primary = {x, y}; analog_input.secondary = {rx, ry};
		analog_input.trigger_left = lt; analog_input.trigger_right = rt;
	}
private:
	class WebAudio final : public GenericAudio {
	public:
		explicit WebAudio(const Game_ConfigAudio& cfg);
		// All engine and decoder callbacks execute on the same Worker.
		void LockMutex() const override {}
		void UnlockMutex() const override {}
	} audio;
};
#endif
