#!/usr/bin/env python3
"""Minimal MPRIS2 player for testing ZlefSDC without a real Spotify.

Owns org.mpris.MediaPlayer2.spotify on the session bus and serves enough of the
MediaPlayer2 + MediaPlayer2.Player interfaces (Metadata, PlaybackStatus,
PlayPause/Next/Previous, CanGoNext/Prev) for the widget to render and drive.
"""
import sys
import os
import time
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

BUS = "org.mpris.MediaPlayer2.spotify"
PATH = "/org/mpris/MediaPlayer2"
ROOT = "org.mpris.MediaPlayer2"
PLAYER = "org.mpris.MediaPlayer2.Player"

ART = sys.argv[1] if len(sys.argv) > 1 else ""

TRACKS = [
    ("Midnight City", ["M83"], "Hurry Up, We're Dreaming"),
    ("Strobe", ["deadmau5"], "For Lack of a Better Name"),
    ("Nightcall", ["Kavinsky", "Lovefoxxx"], "OutRun"),
]


class Player(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, PATH)
        self.i = 0
        self.status = "Playing"
        self.base = 0                 # microseconds at last state change
        self.t0 = time.monotonic()

    def position(self):
        # MOCK_BAD_POSITION=1 simulates Spotify always reporting 0 (the real bug)
        if os.environ.get("MOCK_BAD_POSITION"):
            return dbus.Int64(0)
        pos = self.base
        if self.status == "Playing":
            pos += int((time.monotonic() - self.t0) * 1_000_000)
        return dbus.Int64(max(0, pos))

    # --- Properties -------------------------------------------------------
    def metadata(self):
        t, artists, album = TRACKS[self.i]
        m = {
            "mpris:trackid": dbus.ObjectPath("/track/%d" % self.i),
            # Spotify sends length as uint64 (t), not int64 (x) — mirror that
            "mpris:length": dbus.UInt64(231_000_000),
            "xesam:title": t,
            "xesam:artist": dbus.Array(artists, signature="s"),
            "xesam:album": album,
        }
        if ART:
            m["mpris:artUrl"] = ART
        return dbus.Dictionary(m, signature="sv")

    @dbus.service.method("org.freedesktop.DBus.Properties",
                         in_signature="ss", out_signature="v")
    def Get(self, iface, prop):
        return self.GetAll(iface).get(prop)

    @dbus.service.method("org.freedesktop.DBus.Properties",
                         in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        if iface == ROOT:
            return dbus.Dictionary({
                "Identity": "Spotify",
                "DesktopEntry": "spotify",
                "CanRaise": True,
            }, signature="sv")
        if iface == PLAYER:
            return dbus.Dictionary({
                "PlaybackStatus": self.status,
                "Metadata": self.metadata(),
                "CanGoNext": True,
                "CanGoPrevious": True,
                "CanPlay": True,
                "CanPause": True,
                "Position": self.position(),
            }, signature="sv")
        return dbus.Dictionary({}, signature="sv")

    @dbus.service.signal("org.freedesktop.DBus.Properties",
                         signature="sa{sv}as")
    def PropertiesChanged(self, iface, changed, invalidated):
        pass

    def _emit(self):
        self.PropertiesChanged(PLAYER, {
            "PlaybackStatus": self.status,
            "Metadata": self.metadata(),
        }, [])

    # --- Root -------------------------------------------------------------
    @dbus.service.method(ROOT)
    def Raise(self):
        print("Raise()", flush=True)

    # --- Player methods ---------------------------------------------------
    @dbus.service.method(PLAYER)
    def PlayPause(self):
        if self.status == "Playing":
            self.base += int((time.monotonic() - self.t0) * 1_000_000)
            self.status = "Paused"
        else:
            self.t0 = time.monotonic()
            self.status = "Playing"
        print("PlayPause ->", self.status, flush=True)
        self._emit()

    @dbus.service.method(PLAYER)
    def Next(self):
        self.i = (self.i + 1) % len(TRACKS)
        self.base = 0; self.t0 = time.monotonic()
        print("Next ->", TRACKS[self.i][0], flush=True)
        self._emit()

    @dbus.service.method(PLAYER)
    def Previous(self):
        self.i = (self.i - 1) % len(TRACKS)
        self.base = 0; self.t0 = time.monotonic()
        print("Previous ->", TRACKS[self.i][0], flush=True)
        self._emit()

    @dbus.service.method(PLAYER)
    def Stop(self):
        self.status = "Stopped"
        self._emit()


def main():
    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    name = dbus.service.BusName(BUS, bus)
    Player(bus)
    print("mock MPRIS up on", BUS, "art=" + (ART or "(none)"), flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()
