#include "theme_watcher.h"
#include "platform_color_scheme.h"

#include <slint.h>

#if defined(__linux__)

#include <atomic>
#include <chrono>
#include <thread>

namespace {

std::atomic<bool> g_theme_watcher_running{false};
std::thread g_theme_watcher_thread;

static void sleep_interruptible(std::chrono::milliseconds total, std::atomic<bool>& running)
{
	const auto deadline = std::chrono::steady_clock::now() + total;
	while (std::chrono::steady_clock::now() < deadline
		&& running.load(std::memory_order_relaxed)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

static void theme_watcher_thread_fn(
	slint::ComponentWeakHandle<MainWindow> weak_ui,
	std::optional<slint::cbindgen_private::ColorScheme> last)
{
	while (g_theme_watcher_running.load(std::memory_order_relaxed)) {
		sleep_interruptible(std::chrono::milliseconds(750), g_theme_watcher_running);
		if (!g_theme_watcher_running.load(std::memory_order_relaxed))
			break;

		auto opt = platform_detect_desktop_color_scheme();
		if (!opt)
			continue;
		if (last.has_value() && *last == *opt)
			continue;
		last = *opt;

		if (!g_theme_watcher_running.load(std::memory_order_relaxed))
			break;

		const auto scheme = *opt;
		slint::invoke_from_event_loop([weak_ui, scheme]() {
			if (auto locked = weak_ui.lock())
				(*locked)->set_window_color_scheme(scheme);
		});
	}
}

} // namespace

#endif // __linux__

void start_theme_watcher(
	const slint::ComponentHandle<MainWindow>& ui,
	bool follow_system,
	std::optional<slint::cbindgen_private::ColorScheme> initial_scheme)
{
#if defined(__linux__)
	if (!follow_system)
		return;
	if (g_theme_watcher_thread.joinable())
		return;

	g_theme_watcher_running = true;
	slint::ComponentWeakHandle<MainWindow> weak(ui);
	g_theme_watcher_thread = std::thread(theme_watcher_thread_fn, weak, std::move(initial_scheme));
#else
	(void)ui;
	(void)follow_system;
	(void)initial_scheme;
#endif
}

void stop_theme_watcher()
{
#if defined(__linux__)
	g_theme_watcher_running = false;
	if (g_theme_watcher_thread.joinable())
		g_theme_watcher_thread.join();
#endif
}
