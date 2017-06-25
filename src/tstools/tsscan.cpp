//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2017, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------
//
//  DVB network scanning utility
//
//----------------------------------------------------------------------------

#include "tsArgs.h"
#include "tsCOM.h"
#include "tsTuner.h"
#include "tsTunerParametersDVBT.h"
#include "tsTunerParametersDVBC.h"
#include "tsTunerParametersDVBS.h"
#include "tsTunerParametersATSC.h"
#include "tsTSScanner.h"
#include "tsTime.h"
#include "tsFormat.h"
#include "tsDecimal.h"
#include "tsNullReport.h"

#define MIN_LOCK_TIMEOUT      100 // ms
#define DEFAULT_LOCK_TIMEOUT  800 // ms
#define DEFAULT_PSI_TIMEOUT   5000 // ms
#define DEFAULT_MIN_STRENGTH  10
#define DEFAULT_MIN_QUALITY   10
#define DEFAULT_FIRST_OFFSET  (-2)
#define DEFAULT_LAST_OFFSET   (+2)
#define OFFSET_EXTEND         3


//----------------------------------------------------------------------------
//  Command line options
//----------------------------------------------------------------------------

struct Options: public ts::Args
{
    Options(int argc, char *argv[]);

    std::string     device_name;
    bool            no_offset;
    bool            use_best_quality;
    bool            use_best_strength;
    int             first_uhf_channel;
    int             last_uhf_channel;
    int             first_uhf_offset;
    int             last_uhf_offset;
    int             min_strength;
    int             min_quality;
    bool            show_modulation;
    bool            list_services;
    bool            global_services;
    ts::MilliSecond psi_timeout;
    ts::MilliSecond signal_timeout;
};

Options::Options(int argc, char *argv[]) :
    ts::Args("DVB network scanning utility.", "[options]"),
    device_name(),
    no_offset(false),
    use_best_quality(false),
    use_best_strength(false),
    first_uhf_channel(0),
    last_uhf_channel(0),
    first_uhf_offset(0),
    last_uhf_offset(0),
    min_strength(0),
    min_quality(0),
    show_modulation(false),
    list_services(false),
    global_services(false),
    psi_timeout(0),
    signal_timeout(0)
{
    option("adapter",             'a', UNSIGNED);
    option("best-quality",         0);
    option("best-strength",        0);
    option("debug",                0,  POSITIVE, 0, 1, 0, 0, true);
    option("device-name",         'd', STRING);
    option("first-uhf-channel",   'f', INTEGER, 0, 1, ts::UHF::FIRST_CHANNEL, ts::UHF::LAST_CHANNEL);
    option("first-offset",         0,  INTEGER, 0, 1, -40, +40);
    option("global-service-list", 'g');
    option("last-uhf-channel",    'l', INTEGER, 0, 1, ts::UHF::FIRST_CHANNEL, ts::UHF::LAST_CHANNEL);
    option("last-offset",          0,  INTEGER, 0, 1, -40, +40);
    option("min-quality",          0,  INTEGER, 0, 1, 0, 100);
    option("min-strength",         0,  INTEGER, 0, 1, 0, 100);
    option("modulation",          'm');
    option("no-offset",           'n');
    option("psi-timeout",          0,  UNSIGNED);
    option("service-list",        's');
    option("uhf-band",            'u');
    option("timeout",             't', INTEGER, 0, 1, MIN_LOCK_TIMEOUT, UNLIMITED_VALUE);
    option("verbose",             'v');

    setHelp("Options:\n"
            "\n"
            "  -a N\n"
            "  --adapter N\n"
#if defined(__linux)
            "      Specifies the Linux DVB adapter N (/dev/dvb/adapterN).\n"
#elif defined(__windows)
            "      Specifies the Nth DVB adapter in the system.\n"
#endif
            "      This option can be used instead of device name.\n"
            "      Use the tslsdvb utility to list all DVB devices.\n"
            "\n"
            "  --best-quality\n"
            "      With UHF-band scanning, for each channel, use the offset with the\n"
            "      best signal quality. By default, use the average of lowest and highest\n"
            "      offsets with required minimum quality and strength.\n"
            "\n"
            "  --best-strength\n"
            "      With UHF-band scanning, for each channel, use the offset with the\n"
            "      best signal strength. By default, use the average of lowest and highest\n"
            "      offsets with required minimum quality and strength.\n"
            "\n"
            "  -d \"name\"\n"
            "  --device-name \"name\"\n"
#if defined(__linux)
            "      Specify the DVB receiver device name, /dev/dvb/adapterA[:F[:M[:V]]]\n"
            "      where A = adapter number, F = frontend number (default: 0), M = demux\n"
            "      number (default: 0), V = dvr number (default: 0). The option --adapter\n"
            "      can also be used instead of the device name.\n"
#elif defined(__windows)
            "      Specify the DVB receiver device name. This is a DirectShow/BDA tuner\n"
            "      filter name (not case sensitive, blanks are ignored).\n"
#endif
            "      By default, the first DVB receiver device is used.\n"
            "      Use the tslsdvb utility to list all devices.\n"
            "\n"
            "  -f value\n"
            "  --first-uhf-channel value\n"
            "      For UHF-band scanning, specify the first channel to scan (default: " +
            ts::Decimal(ts::UHF::FIRST_CHANNEL) + ").\n"
            "\n"
            "  --first-offset value\n"
            "      For UHF-band scanning, specify the first offset to scan (default: " +
            ts::Decimal(DEFAULT_FIRST_OFFSET, 0, true, ",", true) + ")\n"
            "      on each channel.\n"
            "\n"
            "  -g\n"
            "  --global-service-list\n"
            "      Same as --service-list but display a global list of services at the end\n"
            "      of scanning instead of per transport stream.\n"
            "\n"
            "  --help\n"
            "      Display this help text.\n"
            "\n"
            "  -l value\n"
            "  --last-uhf-channel value\n"
            "      For UHF-band scanning, specify the last channel to scan (default: " +
            ts::Decimal(ts::UHF::LAST_CHANNEL) + ").\n"
            "\n"
            "  --last-offset value\n"
            "      For UHF-band scanning, specify the last offset to scan (default: " +
            ts::Decimal(DEFAULT_LAST_OFFSET, 0, true, ",", true) + ")\n"
            "      on each channel.\n"
            "\n"
            "  --min-quality value\n"
            "      Minimum signal quality percentage. Frequencies with lower signal\n"
            "      quality are ignored (default: " + ts::Decimal(DEFAULT_MIN_QUALITY) + "%).\n"
            "\n"
            "  --min-strength value\n"
            "      Minimum signal strength percentage. Frequencies with lower signal\n"
            "      strength are ignored (default: " + ts::Decimal(DEFAULT_MIN_STRENGTH) + "%).\n"
            "\n"
            "  -m\n"
            "  --modulation\n"
            "      Display modulation parameters when possible.\n"
            "\n"
            "  -n\n"
            "  --no-offset\n"
            "      For UHF-band scanning, scan only the central frequency of each channel.\n"
            "      Do not scan frequencies with offsets.\n"
            "\n"
            "  --psi-timeout milliseconds\n"
            "      Specifies the timeout, in milli-seconds, for PSI/SI table collection.\n"
            "      Useful only with --service-list. The default is " +
            ts::Decimal(DEFAULT_PSI_TIMEOUT) + " milli-seconds.\n"
            "\n"
            "  -s\n"
            "  --service-list\n"
            "      Read SDT of each channel and display the list of services.\n"
            "\n"
            "  -u\n"
            "  --uhf-band\n"
            "      Perform DVB-T UHF-band scanning. Currently, this is the only supported\n"
            "      scanning method.\n"
            "\n"
            "  -t milliseconds\n"
            "  --timeout milliseconds\n"
            "      Specifies the timeout, in milli-seconds, for DVB signal locking. If no\n"
            "      signal is detected after this timeout, the frequency is skipped. The\n"
            "      default is " + ts::Decimal(DEFAULT_LOCK_TIMEOUT) + " milli-seconds.\n"
            "\n"
            "  -v\n"
            "  --verbose\n"
            "      Produce verbose output.\n"
            "\n"
            "  --version\n"
            "      Display the version number.\n");

    analyze(argc, argv);

    setDebugLevel(present("debug") ? intValue("debug", ts::Severity::Debug) : present("verbose") ? ts::Severity::Verbose : ts::Severity::Info);

    use_best_quality  = present("best-quality");
    use_best_strength = present("best-strength");
    first_uhf_channel = intValue("first-uhf-channel", ts::UHF::FIRST_CHANNEL);
    last_uhf_channel  = intValue("last-uhf-channel", ts::UHF::LAST_CHANNEL);
    show_modulation   = present("modulation");
    no_offset         = present("no-offset");
    first_uhf_offset  = no_offset ? 0 : intValue("first-offset", DEFAULT_FIRST_OFFSET);
    last_uhf_offset   = no_offset ? 0 : intValue("last-offset", DEFAULT_LAST_OFFSET);
    min_quality       = intValue("min-quality", DEFAULT_MIN_QUALITY);
    min_strength      = intValue("min-strength", DEFAULT_MIN_STRENGTH);
    list_services     = present("service-list");
    global_services   = present("global-service-list");
    psi_timeout       = intValue<ts::MilliSecond>("psi-timeout", DEFAULT_PSI_TIMEOUT);
    signal_timeout    = intValue<ts::MilliSecond>("timeout", DEFAULT_LOCK_TIMEOUT);
    device_name       = value("device-name");

    if (present("adapter")) {
        if (device_name.empty()) {
#if defined(__linux)
            device_name = ts::Format("/dev/dvb/adapter%d", intValue("adapter", 0));
#elif defined(__windows)
            device_name = ts::Format(":%d", intValue("adapter", 0));
#endif
        }
        else {
            error("--adapter cannot be used with --device-name");
        }
    }

    exitOnError();
}


//----------------------------------------------------------------------------
//  Analyze and display relevant TS info
//----------------------------------------------------------------------------

namespace {
    void DisplayTS(std::ostream& strm, const std::string& margin, Options& opt, ts::Tuner& tuner, ts::ServiceList& global_services)
    {
        const bool get_services = opt.list_services || opt.global_services;

        // Collect info
        ts::TSScanner info(tuner, opt.psi_timeout, !get_services, opt);

        // Display TS Id
        ts::SafePtr<ts::PAT> pat;
        info.getPAT(pat);
        if (!pat.isNull()) {
            strm << margin
                 << ts::Format("Transport stream id: %d, 0x%04X", int(pat->ts_id), int(pat->ts_id))
                 << std::endl;
        }

        // Display modulation parameters
        ts::TunerParametersPtr tparams;
        info.getTunerParameters(tparams);
        if (opt.show_modulation && !tparams.isNull()) {
            tparams->displayParameters(strm, margin);
        }

        // Display services
        if (get_services) {
            ts::ServiceList services;
            if (info.getServices(services)) {
                if (opt.list_services) {
                    // Display services for this TS
                    services.sort(ts::Service::Sort1);
                    strm << std::endl;
                    ts::Service::Display(strm, margin, services);
                    strm << std::endl;
                }
                if (opt.global_services) {
                    // Add collected services in global service list
                    global_services.insert(global_services.end(), services.begin(), services.end());
                }
            }
        }
    }
}


//----------------------------------------------------------------------------
//  UHF-band offset scanner: Scan offsets around a specific UHF channel and
//  determine offset with the best signal.
//----------------------------------------------------------------------------

class OffsetScanner
{
public:
    // Constructor
    // Perform scanning. Keep signal tuned on best offset
    OffsetScanner(Options& opt, ts::Tuner& tuner, int channel);

    // Check if signal found and which offset is the best one.
    bool signalFound() const {return _signal_found;}
    int channel() const {return _channel;}
    int bestOffset() const {return _best_offset;}

private:
    Options&   _opt;
    ts::Tuner& _tuner;
    const int  _channel;
    bool       _signal_found;
    int        _best_offset;
    int        _lowest_offset;
    int        _highest_offset;
    int        _best_quality;
    int        _best_quality_offset;
    int        _best_strength;
    int        _best_strength_offset;

    // Tune to specified offset. Return false on error.
    bool tune(int offset);

    // Test the signal at one specific offset. Return true if signal is found.
    bool tryOffset(int offset);
};


// Constructor. Perform scanning. Keep signal tuned on best offset
OffsetScanner::OffsetScanner(Options& opt, ts::Tuner& tuner, int channel) :
    _opt(opt),
    _tuner(tuner),
    _channel(channel),
    _signal_found(false),
    _best_offset(0),
    _lowest_offset(0),
    _highest_offset(0),
    _best_quality(0),
    _best_quality_offset(0),
    _best_strength(0),
    _best_strength_offset(0)
{
    _opt.verbose("scanning channel " + ts::Decimal(_channel) + ", " + ts::Decimal(ts::UHF::Frequency(_channel)) + " Hz");

    if (_opt.no_offset) {
        // Only try the central frequency
        tryOffset(0);
    }
    else {
        // Scan lower offsets in descending order, starting at central frequency
        if (_opt.first_uhf_offset <= 0) {
            bool last_ok = false;
            int offset = _opt.last_uhf_offset > 0 ? 0 : _opt.last_uhf_offset;
            while (offset >= _opt.first_uhf_offset - (last_ok ? OFFSET_EXTEND : 0)) {
                last_ok = tryOffset(offset);
                --offset;
            }
        }

        // Scan higher offsets in ascending order, starting after central frequency
        if (_opt.last_uhf_offset > 0) {
            bool last_ok = false;
            int offset = _opt.first_uhf_offset <= 0 ? 1 : _opt.first_uhf_offset;
            while (offset <= _opt.last_uhf_offset + (last_ok ? OFFSET_EXTEND : 0)) {
                last_ok = tryOffset(offset);
                ++offset;
            }
        }
    }

    // If signal was found, select best offset
    if (_signal_found) {
        if (_opt.use_best_quality && _best_quality > 0) {
            // Signal quality indicator is valid, use offset with best signal quality
            _best_offset = _best_quality_offset;
        }
        else if (_opt.use_best_strength && _best_strength > 0) {
            // Signal strength indicator is valid, use offset with best signal strength
            _best_offset = _best_strength_offset;
        }
        else {
            // Default: use average between lowest and highest offsets
            _best_offset = (_lowest_offset + _highest_offset) / 2;
        }

        // Finally, tune back to best offset
        _signal_found = tune(_best_offset);
    }
}


// Tune to specified offset. Return false on error.
bool OffsetScanner::tune(int offset)
{
    // Default tuning parameters
    ts::TunerParametersDVBT tparams;
    tparams.frequency = ts::UHF::Frequency(_channel, offset);
    tparams.inversion = ts::SPINV_AUTO;
#if defined(__windows)
    tparams.bandwidth = ts::BW_8_MHZ; // BW_AUTO not supported
#else
    tparams.bandwidth = ts::BW_AUTO;
#endif
    tparams.fec_hp = ts::FEC_AUTO;
    tparams.fec_lp = ts::FEC_AUTO;
    tparams.modulation = ts::QAM_AUTO;
    tparams.transmission_mode = ts::TM_AUTO;
    tparams.guard_interval = ts::GUARD_AUTO;
    tparams.hierarchy = ts::HIERARCHY_AUTO;
    return _tuner.tune(tparams, _opt);
}


// Test the signal at one specific offset
bool OffsetScanner::tryOffset(int offset)
{
    _opt.debug("trying offset %d", offset);

    // Tune to transponder and start signal acquisition.
    // Signal locking timeout is applied in start().
    if (!tune(offset) || !_tuner.start(_opt)) {
        return false;
    }

    // Previously, we double-checked that the signal was locked.
    // However, looking for signal locked fails on Windows, even if the
    // signal was actually locked. So, we skip this test and we rely on
    // the fact that the signal timeout is always non-zero with tsscan,
    // so since _tuner.start() has succeeded we can be sure that at least
    // one packet was successfully read and there is some signal.
    bool ok =
#if defined(__linux)
        _tuner.signalLocked(_opt);
#else
        true;
#endif

    if (ok) {
        // Get signal quality & strength
        const int strength = _tuner.signalStrength(_opt);
        const int quality = _tuner.signalQuality(_opt);
        _opt.verbose(ts::UHF::Description(_channel, offset, strength, quality));

        if (strength >= 0 && strength <= _opt.min_strength) {
            // Strength is supported but too low
            ok = false;
        }
        else if (strength > _best_strength) {
            // Best offset so far for signal strength
            _best_strength = strength;
            _best_strength_offset = offset;
        }

        if (quality >= 0 && quality <= _opt.min_quality) {
            // Quality is supported but too low
            ok = false;
        }
        else if (quality > _best_quality) {
            // Best offset so far for signal quality
            _best_quality = quality;
            _best_quality_offset = offset;
        }
    }

    if (ok) {
        if (!_signal_found) {
            // First offset with signal on this channel
            _signal_found = true;
            _lowest_offset = _highest_offset = offset;
        }
        else if (offset < _lowest_offset) {
            _lowest_offset = offset;
        }
        else if (offset > _highest_offset) {
            _highest_offset = offset;
        }
    }

    // Stop signal acquisition
    _tuner.stop(_opt);

    return ok;
}


//----------------------------------------------------------------------------
//  UHF-band scanning
//----------------------------------------------------------------------------

namespace {
    int UHFScan(Options& opt, ts::Tuner& tuner)
    {
        ts::ServiceList all_services;

        // UHF means DVB-T
        if (tuner.tunerType() != ts::DVB_T) {
            opt.error("UHF scanning needs DVB-T, tuner " + tuner.deviceName() + " is " + ts::TunerTypeEnum.name(tuner.tunerType()));
            return EXIT_FAILURE;
        }

        // Loop on all selected UHF channels
        for (int chan = opt.first_uhf_channel; chan <= opt.last_uhf_channel; ++chan) {

            // Scan all offsets surrounding the channel
            OffsetScanner offscan(opt, tuner, chan);
            if (offscan.signalFound()) {

                // Report channel characteristics
                std::cout << "* UHF "
                          << ts::UHF::Description(chan, offscan.bestOffset(), tuner.signalStrength(opt), tuner.signalQuality(opt))
                          << std::endl;

                // Analyze PSI/SI if required
                DisplayTS(std::cout, "  ", opt, tuner, all_services);
            }
        }

        // Report global list of services if required
        if (opt.global_services) {
            all_services.sort(ts::Service::Sort1);
            std::cout << std::endl;
            ts::Service::Display(std::cout, "", all_services);
        }

        return EXIT_SUCCESS;
    }
}


//----------------------------------------------------------------------------
//  Program entry point
//----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    Options opt(argc, argv);
    ts::COM com(opt);
    ts::Tuner tuner(opt.device_name, false, opt);

    tuner.setSignalTimeout(opt.signal_timeout);
    tuner.setSignalTimeoutSilent(true);
    tuner.setReceiveTimeout(opt.psi_timeout, opt);

    // Only one currently supported mode: UHF-band scanning
    return UHFScan(opt, tuner);
}
