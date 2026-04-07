// X11 CLIPBOARD selection owner (ICCCM). Adapted from the selection-request flow used by GLFW
// (zlib/libpng license). Runs on a detached thread with its own Display so Slint's connection
// is unaffected.

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace apikulture::clipboard::detail {

namespace {

void clipboard_x11_thread(std::string payload) {
	Display* d = XOpenDisplay(nullptr);
	if (!d) return;

	const int screen = DefaultScreen(d);
	Window root = RootWindow(d, screen);
	Window w = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0);
	if (!w) {
		XCloseDisplay(d);
		return;
	}

	Atom clipboard = XInternAtom(d, "CLIPBOARD", False);
	Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
	Atom targets_atom = XInternAtom(d, "TARGETS", False);
	Atom save_targets = XInternAtom(d, "SAVE_TARGETS", False);
	Atom null_atom = XInternAtom(d, "NULL", False);
	Atom timestamp_atom = XInternAtom(d, "TIMESTAMP", False);
	const Atom xa_string = XA_STRING;

	XSetSelectionOwner(d, clipboard, w, CurrentTime);
	XFlush(d);
	if (XGetSelectionOwner(d, clipboard) != w) {
		XDestroyWindow(d, w);
		XCloseDisplay(d);
		return;
	}

	const auto thread_start = std::chrono::steady_clock::now();
	constexpr auto k_max_lifetime = std::chrono::minutes(5);
	constexpr auto k_idle_exit = std::chrono::seconds(120);
	auto last_activity = thread_start;

	while (true) {
		const auto now = std::chrono::steady_clock::now();
		if (now - thread_start > k_max_lifetime) break;

		if (XPending(d) == 0) {
			if (now - last_activity > k_idle_exit) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}

		XEvent ev;
		XNextEvent(d, &ev);
		last_activity = std::chrono::steady_clock::now();

		if (ev.type == SelectionClear) {
			break;
		}
		if (ev.type != SelectionRequest) {
			continue;
		}

		const XSelectionRequestEvent* req = &ev.xselectionrequest;
		XSelectionEvent reply {};
		reply.type = SelectionNotify;
		reply.serial = 0;
		reply.send_event = True;
		reply.display = req->display;
		reply.requestor = req->requestor;
		reply.selection = req->selection;
		reply.target = req->target;
		reply.time = req->time;
		reply.property = None;

		if (req->property == None) {
			// ICCCM legacy requestors (property None); decline.
		} else if (req->target == targets_atom) {
			const Atom tgs[] = {targets_atom, utf8, xa_string};
			XChangeProperty(d, req->requestor, req->property, XA_ATOM, 32, PropModeReplace,
					reinterpret_cast<const unsigned char*>(tgs),
					static_cast<int>(sizeof(tgs) / sizeof(Atom)));
			reply.property = req->property;
		} else if (req->target == save_targets) {
			XChangeProperty(d, req->requestor, req->property, null_atom, 32, PropModeReplace,
					nullptr, 0);
			reply.property = req->property;
		} else if (req->target == timestamp_atom) {
			const uint32_t t = static_cast<uint32_t>(CurrentTime);
			XChangeProperty(d, req->requestor, req->property, XA_INTEGER, 32, PropModeReplace,
					reinterpret_cast<const unsigned char*>(&t), 1);
			reply.property = req->property;
		} else if (req->target == utf8 || req->target == xa_string) {
			XChangeProperty(d, req->requestor, req->property, req->target, 8, PropModeReplace,
					reinterpret_cast<const unsigned char*>(payload.data()),
					static_cast<int>(payload.size()));
			reply.property = req->property;
		}

		XEvent out {};
		out.xselection = reply;
		XSendEvent(d, req->requestor, False, NoEventMask, &out);
		XFlush(d);
	}

	XDestroyWindow(d, w);
	XCloseDisplay(d);
}

}  // namespace

bool set_utf8_x11(std::string_view text) {
	try {
		std::string copy(text.data(), text.size());
		std::thread(clipboard_x11_thread, std::move(copy)).detach();
		return true;
	} catch (...) {
		return false;
	}
}

}  // namespace apikulture::clipboard::detail
