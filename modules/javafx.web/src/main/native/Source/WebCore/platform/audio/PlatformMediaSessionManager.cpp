/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformMediaSessionManager.h"

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "NowPlayingInfo.h"
#include "PlatformMediaSession.h"
#if PLATFORM(COCOA)
#include "VP9UtilitiesCocoa.h"
#endif

namespace WebCore {


#if ENABLE(WEBM_FORMAT_READER)
bool PlatformMediaSessionManager::m_webMFormatReaderEnabled;
#endif

#if ENABLE(VORBIS)
bool PlatformMediaSessionManager::m_vorbisDecoderEnabled;
#endif

#if ENABLE(OPUS)
bool PlatformMediaSessionManager::m_opusDecoderEnabled;
#endif

#if ENABLE(ALTERNATE_WEBM_PLAYER)
bool PlatformMediaSessionManager::m_alternateWebMPlayerEnabled;
#endif

#if HAVE(SC_CONTENT_SHARING_PICKER)
bool PlatformMediaSessionManager::s_useSCContentSharingPicker;
#endif

#if ENABLE(VP9)
bool PlatformMediaSessionManager::m_vp9DecoderEnabled;
bool PlatformMediaSessionManager::m_vp8DecoderEnabled;
bool PlatformMediaSessionManager::m_swVPDecodersAlwaysEnabled;
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
bool PlatformMediaSessionManager::s_mediaCapabilityGrantsEnabled;
#endif

static std::unique_ptr<PlatformMediaSessionManager>& sharedPlatformMediaSessionManager()
{
    static NeverDestroyed<std::unique_ptr<PlatformMediaSessionManager>> platformMediaSessionManager;
    return platformMediaSessionManager.get();
}

PlatformMediaSessionManager& PlatformMediaSessionManager::sharedManager()
{
    auto& manager = sharedPlatformMediaSessionManager();
    if (!manager) {
        manager = PlatformMediaSessionManager::create();
        manager->resetRestrictions();
    }
    return *manager;
}

PlatformMediaSessionManager* PlatformMediaSessionManager::sharedManagerIfExists()
{
    return sharedPlatformMediaSessionManager().get();
}

#if !PLATFORM(COCOA) && (!USE(GLIB) || !ENABLE(MEDIA_SESSION))
std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    return std::unique_ptr<PlatformMediaSessionManager>(new PlatformMediaSessionManager);
}
#endif // !PLATFORM(COCOA) && (!USE(GLIB) || !ENABLE(MEDIA_SESSION))

void PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary()
{
    if (auto existingManager = PlatformMediaSessionManager::sharedManagerIfExists())
        existingManager->scheduleSessionStatusUpdate();
}

void PlatformMediaSessionManager::updateAudioSessionCategoryIfNecessary()
{
    if (auto existingManager = PlatformMediaSessionManager::sharedManagerIfExists())
        existingManager->scheduleUpdateSessionState();
}

PlatformMediaSessionManager::PlatformMediaSessionManager()
#if !RELEASE_LOG_DISABLED
    : m_stateLogTimer(makeUniqueRef<Timer>(*this, &PlatformMediaSessionManager::dumpSessionStates))
    , m_logger(AggregateLogger::create(this))
#endif
{
}

PlatformMediaSessionManager::~PlatformMediaSessionManager()
{
    m_taskGroup.cancel();
}

static inline unsigned indexFromMediaType(PlatformMediaSession::MediaType type)
{
    return static_cast<unsigned>(type);
}

void PlatformMediaSessionManager::resetRestrictions()
{
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Video)] = NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Audio)] = NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::VideoAudio)] = NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::WebAudio)] = NoRestrictions;
}

bool PlatformMediaSessionManager::has(PlatformMediaSession::MediaType type) const
{
    return anyOfSessions([type] (auto& session) {
        return session.mediaType() == type;
    });
}

bool PlatformMediaSessionManager::activeAudioSessionRequired() const
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (anyOfSessions([] (auto& session) { return session.activeAudioSessionRequired(); }))
        return true;

    return WTF::anyOf(m_audioCaptureSources, [](auto& source) {
        return source.isCapturingAudio();
    });
#else
    return false;
#endif
}

bool PlatformMediaSessionManager::hasActiveAudioSession() const
{
#if USE(AUDIO_SESSION)
    return m_becameActive;
#else
    return true;
#endif
}

bool PlatformMediaSessionManager::canProduceAudio() const
{
    return anyOfSessions([] (auto& session) {
        return session.canProduceAudio();
    });
}

std::optional<NowPlayingInfo> PlatformMediaSessionManager::nowPlayingInfo() const
{
    return { };
}

int PlatformMediaSessionManager::count(PlatformMediaSession::MediaType type) const
{
    int count = 0;
    for (const auto& session : m_sessions) {
        if (session->mediaType() == type)
            ++count;
    }

    return count;
}

int PlatformMediaSessionManager::countActiveAudioCaptureSources()
{
    int count = 0;
    for (const auto& source : m_audioCaptureSources) {
        if (source.wantsToCaptureAudio())
            ++count;
    }
    return count;
}

void PlatformMediaSessionManager::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_currentInterruption = type;
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([type] (auto& session) {
        session.beginInterruption(type);
    });
#endif
    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_currentInterruption = { };
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([flags] (auto& session) {
        session.endInterruption(flags);
    });
#endif
}

void PlatformMediaSessionManager::addSession(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    m_sessions.append(session);
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (m_currentInterruption)
        session.beginInterruption(*m_currentInterruption);
#endif

#if !RELEASE_LOG_DISABLED
    m_logger->addLogger(session.logger());
#endif

    scheduleUpdateSessionState();
}

bool PlatformMediaSessionManager::hasNoSession() const
{
    return m_sessions.isEmpty() || std::all_of(m_sessions.begin(), m_sessions.end(), std::logical_not<void>());
}

void PlatformMediaSessionManager::removeSession(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    size_t index = m_sessions.find(&session);
    if (index == notFound)
        return;

    m_sessions.remove(index);

    if (hasNoSession() && !activeAudioSessionRequired())
        maybeDeactivateAudioSession();

#if !RELEASE_LOG_DISABLED
    m_logger->removeLogger(session.logger());
#endif

    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::addRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)] |= restriction;
}

void PlatformMediaSessionManager::removeRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)] &= ~restriction;
}

PlatformMediaSessionManager::SessionRestrictions PlatformMediaSessionManager::restrictions(PlatformMediaSession::MediaType type)
{
    return m_restrictions[indexFromMediaType(type)];
}

bool PlatformMediaSessionManager::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    setCurrentSession(session);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    auto sessionType = session.mediaType();
    auto restrictions = this->restrictions(sessionType);
    if (session.state() == PlatformMediaSession::State::Interrupted && restrictions & InterruptedPlaybackNotPermitted) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false because session.state() is Interrupted, and InterruptedPlaybackNotPermitted");
        return false;
    }

    if (!maybeActivateAudioSession()) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false, failed to activate AudioSession");
        return false;
    }

    if (m_currentInterruption)
        endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);

    if (restrictions & ConcurrentPlaybackNotPermitted) {
        forEachMatchingSession([&session](auto& oneSession) {
            return &oneSession != &session
                && oneSession.state() == PlatformMediaSession::State::Playing
                && !oneSession.canPlayConcurrently(session);
        }, [](auto& oneSession) {
            oneSession.pauseSession();
        });
    }
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning true");
    return true;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::sessionWillEndPlayback(PlatformMediaSession& session, DelayCallingUpdateNowPlaying)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    if (m_sessions.size() < 2)
        return;

    size_t pausingSessionIndex = notFound;
    size_t lastPlayingSessionIndex = notFound;
    for (size_t i = 0, size = m_sessions.size(); i < size; ++i) {
        const auto& oneSession = *m_sessions[i];
        if (&oneSession == &session)
            pausingSessionIndex = i;
        else if (oneSession.state() == PlatformMediaSession::State::Playing)
            lastPlayingSessionIndex = i;
        else
            break;
    }

    if (lastPlayingSessionIndex == notFound || pausingSessionIndex == notFound)
        return;

    if (pausingSessionIndex > lastPlayingSessionIndex)
        return;

    m_sessions.remove(pausingSessionIndex);
    m_sessions.append(session);

    ALWAYS_LOG(LOGIDENTIFIER, "session moved from index ", pausingSessionIndex, " to ", lastPlayingSessionIndex);
}

void PlatformMediaSessionManager::sessionStateChanged(PlatformMediaSession& session)
{
    // Call updateSessionState() synchronously if the new state is Playing to ensure
    // the audio session is active and has the correct category before playback starts.
    if (session.state() == PlatformMediaSession::State::Playing)
        updateSessionState();
    else
        scheduleUpdateSessionState();

#if !RELEASE_LOG_DISABLED
    scheduleStateLog();
#endif
}

void PlatformMediaSessionManager::setCurrentSession(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    if (m_sessions.size() < 2)
        return;

    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (!index || index == notFound)
        return;

    m_sessions.remove(index);
    m_sessions.insert(0, session);

    ALWAYS_LOG(LOGIDENTIFIER, "session moved from index ", index, " to 0");
}

PlatformMediaSession* PlatformMediaSessionManager::currentSession() const
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0].get();
}

void PlatformMediaSessionManager::applicationWillBecomeInactive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()) & InactiveProcessPlaybackRestricted;
    }, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::ProcessInactive);
    });
}

void PlatformMediaSessionManager::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()) & InactiveProcessPlaybackRestricted;
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::applicationDidEnterBackground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, "suspendedUnderLock: ", suspendedUnderLock);

    if (m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = true;

    forEachSession([&] (auto& session) {
        if (suspendedUnderLock && restrictions(session.mediaType()) & SuspendedUnderLockPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::InterruptionType::SuspendedUnderLock);
        else if (restrictions(session.mediaType()) & BackgroundProcessPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
    });
}

void PlatformMediaSessionManager::applicationWillEnterForeground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, "suspendedUnderLock: ", suspendedUnderLock);

    if (!m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = false;

    forEachMatchingSession([&](auto& session) {
        return (suspendedUnderLock && restrictions(session.mediaType()) & SuspendedUnderLockPlaybackRestricted) || restrictions(session.mediaType()) & BackgroundProcessPlaybackRestricted;
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::processWillSuspend()
{
    if (m_processIsSuspended)
        return;
    m_processIsSuspended = true;

    ALWAYS_LOG(LOGIDENTIFIER);

    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });

    maybeDeactivateAudioSession();
}

void PlatformMediaSessionManager::processDidResume()
{
    if (!m_processIsSuspended)
        return;
    m_processIsSuspended = false;

    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });

#if USE(AUDIO_SESSION)
    if (!m_becameActive)
        maybeActivateAudioSession();
#endif
}

void PlatformMediaSessionManager::setIsPlayingToAutomotiveHeadUnit(bool isPlayingToAutomotiveHeadUnit)
{
    if (isPlayingToAutomotiveHeadUnit == m_isPlayingToAutomotiveHeadUnit)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, isPlayingToAutomotiveHeadUnit);
    m_isPlayingToAutomotiveHeadUnit = isPlayingToAutomotiveHeadUnit;
}

void PlatformMediaSessionManager::setSupportsSpatialAudioPlayback(bool supportsSpatialAudioPlayback)
{
    if (supportsSpatialAudioPlayback == m_supportsSpatialAudioPlayback)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, supportsSpatialAudioPlayback);
    m_supportsSpatialAudioPlayback = supportsSpatialAudioPlayback;
}

std::optional<bool> PlatformMediaSessionManager::supportsSpatialAudioPlaybackForConfiguration(const MediaConfiguration&)
{
    return m_supportsSpatialAudioPlayback;
}

void PlatformMediaSessionManager::sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSession& session)
{
    if (!m_isApplicationInBackground || !(restrictions(session.mediaType()) & BackgroundProcessPlaybackRestricted))
        return;

    if (session.state() != PlatformMediaSession::State::Interrupted)
        session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
}

void PlatformMediaSessionManager::sessionCanProduceAudioChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_alreadyScheduledSessionStatedUpdate)
        return;

    m_alreadyScheduledSessionStatedUpdate = true;
    enqueueTaskOnMainThread([this] {
        m_alreadyScheduledSessionStatedUpdate = false;
    maybeActivateAudioSession();
    updateSessionState();
    });
}

void PlatformMediaSessionManager::processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession || !activeSession->canReceiveRemoteControlCommands())
        return;
    activeSession->didReceiveRemoteControlCommand(command, argument);
}

bool PlatformMediaSessionManager::computeSupportsSeeking() const
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession)
        return false;
    return activeSession->supportsSeeking();
}

void PlatformMediaSessionManager::processSystemWillSleep()
{
    if (m_currentInterruption)
        return;

    forEachSession([] (auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::SystemSleep);
    });
}

void PlatformMediaSessionManager::processSystemDidWake()
{
    if (m_currentInterruption)
        return;

    forEachSession([] (auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::pauseAllMediaPlaybackForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.pauseSession();
    });
}


bool PlatformMediaSessionManager::mediaPlaybackIsPaused(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    bool mediaPlaybackIsPaused = false;
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&mediaPlaybackIsPaused](auto& session) {
        if (session.state() == PlatformMediaSession::State::Paused)
            mediaPlaybackIsPaused = true;
    });
    return mediaPlaybackIsPaused;
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForProcess()
{
    forEachSession([] (auto& session) {
        session.stopSession();
    });
}

void PlatformMediaSessionManager::suspendAllMediaPlaybackForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::PlaybackSuspended);
    });
}

void PlatformMediaSessionManager::resumeAllMediaPlaybackForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::suspendAllMediaBufferingForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.suspendBuffering();
    });
}

void PlatformMediaSessionManager::resumeAllMediaBufferingForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.resumeBuffering();
    });
}

Vector<WeakPtr<PlatformMediaSession>> PlatformMediaSessionManager::sessionsMatching(const Function<bool(const PlatformMediaSession&)>& filter) const
{
    Vector<WeakPtr<PlatformMediaSession>> matchingSessions;
    for (auto& session : m_sessions) {
        if (filter(*session))
            matchingSessions.append(session);
    }
    return matchingSessions;
}

void PlatformMediaSessionManager::forEachMatchingSession(const Function<bool(const PlatformMediaSession&)>& predicate, const Function<void(PlatformMediaSession&)>& callback)
{
    for (auto& session : sessionsMatching(predicate)) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

void PlatformMediaSessionManager::forEachSessionInGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier, const Function<void(PlatformMediaSession&)>& callback)
{
    if (!mediaSessionGroupIdentifier)
        return;

    forEachMatchingSession([mediaSessionGroupIdentifier](auto& session) {
        return session.client().mediaSessionGroupIdentifier() == mediaSessionGroupIdentifier;
    }, [&callback](auto& session) {
        callback(session);
    });
}

void PlatformMediaSessionManager::forEachSession(const Function<void(PlatformMediaSession&)>& callback)
{
    auto sessions = m_sessions;
    for (auto& session : sessions) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

bool PlatformMediaSessionManager::anyOfSessions(const Function<bool(const PlatformMediaSession&)>& predicate) const
{
    return WTF::anyOf(m_sessions, [&predicate](const auto& session) {
        return predicate(*session);
    });
}

void PlatformMediaSessionManager::addAudioCaptureSource(AudioCaptureSource& source)
{
    ASSERT(!m_audioCaptureSources.contains(source));
    m_audioCaptureSources.add(source);
    updateSessionState();
}


void PlatformMediaSessionManager::removeAudioCaptureSource(AudioCaptureSource& source)
{
    m_audioCaptureSources.remove(source);
    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::scheduleUpdateSessionState()
{
    if (m_hasScheduledSessionStateUpdate)
        return;

    m_hasScheduledSessionStateUpdate = true;
    enqueueTaskOnMainThread([this] {
        updateSessionState();
        m_hasScheduledSessionStateUpdate = false;
    });
}

void PlatformMediaSessionManager::maybeDeactivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!m_becameActive || !shouldDeactivateAudioSession())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "tried to set inactive AudioSession");
    AudioSession::sharedSession().tryToSetActive(false);
    m_becameActive = false;
#endif
}

bool PlatformMediaSessionManager::maybeActivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!activeAudioSessionRequired()) {
        ALWAYS_LOG(LOGIDENTIFIER, "active audio session not required");
        return true;
    }

    m_becameActive = AudioSession::sharedSession().tryToSetActive(true);
    ALWAYS_LOG(LOGIDENTIFIER, m_becameActive ? "successfully activated" : "failed to activate", " AudioSession");
    return m_becameActive;
#else
    return true;
#endif
}
static bool& deactivateAudioSession()
{
    static bool deactivate;
    return deactivate;
}

bool PlatformMediaSessionManager::shouldDeactivateAudioSession()
{
    return deactivateAudioSession();
}

void PlatformMediaSessionManager::setShouldDeactivateAudioSession(bool deactivate)
{
    deactivateAudioSession() = deactivate;
}

bool PlatformMediaSessionManager::webMFormatReaderEnabled()
{
#if ENABLE(WEBM_FORMAT_READER)
    return m_webMFormatReaderEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setWebMFormatReaderEnabled(bool enabled)
{
#if ENABLE(WEBM_FORMAT_READER)
    m_webMFormatReaderEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

bool PlatformMediaSessionManager::vorbisDecoderEnabled()
{
#if ENABLE(VORBIS)
    return m_vorbisDecoderEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setVorbisDecoderEnabled(bool enabled)
{
#if ENABLE(VORBIS)
    m_vorbisDecoderEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

bool PlatformMediaSessionManager::opusDecoderEnabled()
{
#if ENABLE(OPUS)
    return m_opusDecoderEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setOpusDecoderEnabled(bool enabled)
{
#if ENABLE(OPUS)
    m_opusDecoderEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

void PlatformMediaSessionManager::setAlternateWebMPlayerEnabled(bool enabled)
{
#if ENABLE(ALTERNATE_WEBM_PLAYER)
    m_alternateWebMPlayerEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

bool PlatformMediaSessionManager::alternateWebMPlayerEnabled()
{
#if ENABLE(ALTERNATE_WEBM_PLAYER)
    return m_alternateWebMPlayerEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setUseSCContentSharingPicker(bool use)
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    s_useSCContentSharingPicker = use;
#else
    UNUSED_PARAM(use);
#endif
}

bool PlatformMediaSessionManager::useSCContentSharingPicker()
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    return s_useSCContentSharingPicker;
#else
    return false;
#endif
}

#if ENABLE(VP9)
void PlatformMediaSessionManager::setShouldEnableVP9Decoder(bool vp9DecoderEnabled)
{
    m_vp9DecoderEnabled = vp9DecoderEnabled;
}

bool PlatformMediaSessionManager::shouldEnableVP9Decoder()
{
    return m_vp9DecoderEnabled;
}

void PlatformMediaSessionManager::setShouldEnableVP8Decoder(bool vp8DecoderEnabled)
{
    m_vp8DecoderEnabled = vp8DecoderEnabled;
}

bool PlatformMediaSessionManager::shouldEnableVP8Decoder()
{
    return m_vp8DecoderEnabled;
}

void PlatformMediaSessionManager::setSWVPDecodersAlwaysEnabled(bool swVPDecodersAlwaysEnabled)
{
    m_swVPDecodersAlwaysEnabled = swVPDecodersAlwaysEnabled;
#if PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setSWVPDecodersAlwaysEnabled(swVPDecodersAlwaysEnabled);
#endif
}

bool PlatformMediaSessionManager::swVPDecodersAlwaysEnabled()
{
    return m_swVPDecodersAlwaysEnabled;
}
#endif // ENABLE(VP9)





#if ENABLE(EXTENSION_CAPABILITIES)
bool PlatformMediaSessionManager::mediaCapabilityGrantsEnabled()
{
    return s_mediaCapabilityGrantsEnabled;
}

void PlatformMediaSessionManager::setMediaCapabilityGrantsEnabled(bool mediaCapabilityGrantsEnabled)
{
    s_mediaCapabilityGrantsEnabled = mediaCapabilityGrantsEnabled;
}
#endif

WeakPtr<PlatformMediaSession> PlatformMediaSessionManager::bestEligibleSessionForRemoteControls(const Function<bool(const PlatformMediaSession&)>& filterFunction, PlatformMediaSession::PlaybackControlsPurpose purpose)
{
    Vector<WeakPtr<PlatformMediaSession>> eligibleAudioVideoSessions;
    Vector<WeakPtr<PlatformMediaSession>> eligibleWebAudioSessions;
    forEachMatchingSession(filterFunction, [&](auto& session) {
        if (session.presentationType() == PlatformMediaSession::MediaType::WebAudio) {
            if (eligibleAudioVideoSessions.isEmpty())
                eligibleWebAudioSessions.append(session);
        } else
            eligibleAudioVideoSessions.append(session);
    });

    if (eligibleAudioVideoSessions.isEmpty()) {
        if (eligibleWebAudioSessions.isEmpty())
            return nullptr;
        return eligibleWebAudioSessions[0]->selectBestMediaSession(eligibleWebAudioSessions, purpose);
    }

    return eligibleAudioVideoSessions[0]->selectBestMediaSession(eligibleAudioVideoSessions, purpose);
}

void PlatformMediaSessionManager::addNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(!m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.add(observer);
    observer(nowPlayingInfo().value_or(NowPlayingInfo { }).metadata);
}

void PlatformMediaSessionManager::removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.remove(observer);
}

void PlatformMediaSessionManager::nowPlayingMetadataChanged(const NowPlayingMetadata& metadata)
{
    m_nowPlayingMetadataObservers.forEach([&] (auto& observer) {
        observer(metadata);
    });
}

bool PlatformMediaSessionManager::hasActiveNowPlayingSessionInGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    bool hasActiveNowPlayingSession = false;

    forEachSessionInGroup(mediaSessionGroupIdentifier, [&](auto& session) {
        hasActiveNowPlayingSession |= session.isActiveNowPlayingSession();
    });

    return hasActiveNowPlayingSession;
}

void PlatformMediaSessionManager::enqueueTaskOnMainThread(Function<void()>&& task)
{
    callOnMainThread(CancellableTask(m_taskGroup, [task = WTFMove(task)] () mutable {
        task();
    }));
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PlatformMediaSessionManager::logChannel() const
{
    return LogMedia;
}

void PlatformMediaSessionManager::scheduleStateLog()
{
    if (m_stateLogTimer->isActive())
        return;

    static constexpr Seconds StateLogDelay { 5_s };
    m_stateLogTimer->startOneShot(StateLogDelay);
}

void PlatformMediaSessionManager::dumpSessionStates()
{
    StringBuilder builder;

    forEachSession([&](auto& session) {
        builder.append('(', hex(reinterpret_cast<uintptr_t>(session.logIdentifier())), "): "_s, session.description(), "\n"_s);
    });

    ALWAYS_LOG(LOGIDENTIFIER, " Sessions:\n", builder.toString());
}
#endif

} // namespace WebCore
