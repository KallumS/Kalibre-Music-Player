// Media keys and Now Playing on the Mac: MPRemoteCommandCenter and MPNowPlayingInfoCenter.
#include "MediaKeys.h"
#include "Controller.h"
#include "TitleFormat.h"

#import <MediaPlayer/MediaPlayer.h>

namespace kp::media
{
namespace
{
struct NowPlaying : public Controller::Listener, private juce::Timer
{
    Controller& c;
    explicit NowPlaying (Controller& x) : c (x) { c.addListener (this); startTimer (2000); update(); }
    ~NowPlaying() override { c.removeListener (this); }
    void playbackChanged() override { update(); }
    void timerCallback() override { update(); }

    void update()
    {
        auto* centre = [MPNowPlayingInfoCenter defaultCenter];
        auto t = c.nowPlayingTrack();
        if (! t)
        {
            centre.nowPlayingInfo = nil;
            centre.playbackState = MPNowPlayingPlaybackStateStopped;
            return;
        }
        auto str = [] (const std::string& s) { return [NSString stringWithUTF8String: s.c_str()]; };
        NSMutableDictionary* info = [NSMutableDictionary dictionary];
        info[MPMediaItemPropertyTitle] = str (tf::format ("%title%", *t));
        info[MPMediaItemPropertyArtist] = str (tf::format ("[%artist%]", *t));
        info[MPMediaItemPropertyAlbumTitle] = str (tf::format ("[%album%]", *t));
        info[MPMediaItemPropertyPlaybackDuration] = @(c.player.length());
        info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(c.player.position());
        info[MPNowPlayingInfoPropertyPlaybackRate] = @(c.paused() ? 0.0 : 1.0);
        centre.nowPlayingInfo = info;
        centre.playbackState = c.paused() ? MPNowPlayingPlaybackStatePaused : MPNowPlayingPlaybackStatePlaying;
    }
};

std::unique_ptr<NowPlaying> nowPlaying;
} // namespace

void install (Controller& c)
{
    auto* centre = [MPRemoteCommandCenter sharedCommandCenter];
    Controller* ctl = &c;
    auto on = [] (MPRemoteCommand* command, std::function<void()> f)
    {
        [command setEnabled: YES];
        [command addTargetWithHandler: ^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
        {
            juce::MessageManager::callAsync (f);
            return MPRemoteCommandHandlerStatusSuccess;
        }];
    };
    on (centre.togglePlayPauseCommand, [ctl] { ctl->playPause(); });
    on (centre.playCommand, [ctl] { ctl->play(); });
    on (centre.pauseCommand, [ctl] { ctl->pause(); });
    on (centre.stopCommand, [ctl] { ctl->stop(); });
    on (centre.nextTrackCommand, [ctl] { ctl->next(); });
    on (centre.previousTrackCommand, [ctl] { ctl->previous(); });
    [centre.changePlaybackPositionCommand setEnabled: YES];
    [centre.changePlaybackPositionCommand addTargetWithHandler: ^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent* e)
    {
        const double to = ((MPChangePlaybackPositionCommandEvent*) e).positionTime;
        juce::MessageManager::callAsync ([ctl, to] { ctl->seek (to); });
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    nowPlaying = std::make_unique<NowPlaying> (c);
}

void uninstall()
{
    nowPlaying.reset();
    auto* centre = [MPRemoteCommandCenter sharedCommandCenter];
    for (MPRemoteCommand* command in @[ centre.togglePlayPauseCommand, centre.playCommand, centre.pauseCommand, centre.stopCommand,
                                         centre.nextTrackCommand, centre.previousTrackCommand, centre.changePlaybackPositionCommand ])
        [command removeTarget: nil];
}

} // namespace kp::media
