#ifndef HW_USB_OVER_SERIAL_HOST
#ifdef TESTS_HW_SUPPORT

#include "framework/Framework.h"
#include <string>
#include <filesystem>
#include "helpers/Misc.h"
#include "helpers/MIDI.h"
#include "stubs/Database.h"
#include <HWTestDefines.h>
#include <Target.h>
#include "system/Builder.h"

namespace
{
    class HWTest : public ::testing::Test
    {
        protected:
        void SetUp()
        {
            LOG(INFO) << "Setting up test";

#ifdef TEST_FLASHING
            static bool flashed = false;
#endif

            // dummy db - used only to retrieve correct amount of supported presets
            ASSERT_TRUE(_database._instance.init());

#ifdef TEST_FLASHING
            if (!flashed)
            {
                LOG(INFO) << "Device not flashed. Starting flashing procedure.";
                cyclePower(powerCycleType_t::STANDARD);

                LOG(INFO) << "Waiting " << turn_on_delay_ms << " ms to ensure target and programmer are available.";
                test::sleepMs(turn_on_delay_ms);

                flash();
                flashed = true;
            }
            else
#endif
            {
                cyclePower(powerCycleType_t::STANDARD_WITH_DEVICE_CHECK);
            }

            factoryReset();
        }

        void TearDown()
        {
            LOG(INFO) << "Tearing down test";

            test::wsystem("rm -f " + temp_midi_data_location);
            test::wsystem("rm -f " + backup_file_location);
            test::wsystem("killall amidi > /dev/null 2>&1");
            test::wsystem("killall ssterm > /dev/null 2>&1");

            std::string cmdResponse;

            if (test::wsystem("pgrep ssterm", cmdResponse) == 0)
            {
                LOG(INFO) << "Waiting for ssterm process to be terminated";

                while (test::wsystem("pgrep ssterm", cmdResponse) == 0)
                    ;
            }

            powerOff();
        }

        const std::string flash_cmd = "make --no-print-directory -C ../src flash ";

        enum class powerCycleType_t : uint8_t
        {
            STANDARD,
            STANDARD_WITH_DEVICE_CHECK
        };

        enum class softRebootType_t : uint8_t
        {
            STANDARD,
            FACTORY_RESET,
            BOOTLOADER,
        };

        enum class flashType_t : uint8_t
        {
            DEVELOPMENT,
            RELEASE,
        };

        void powerOn()
        {
            // Send newline to arduino controller to make sure on/off commands
            // are properly parsed.
            // Add slight delay between power commands to avoid issues.
            ASSERT_EQ(0, test::wsystem("echo > /dev/actl && sleep 1"));

            LOG(INFO) << "Turning USB devices on";
            ASSERT_EQ(0, test::wsystem(usb_power_on_cmd));
        }

        void powerOff()
        {
            // Send newline to arduino controller to make sure on/off commands
            // are properly parsed.
            // Add slight delay between power commands to avoid issues.
            ASSERT_EQ(0, test::wsystem("echo > /dev/actl && sleep 1"));

            LOG(INFO) << "Turning USB devices off";
            ASSERT_EQ(0, test::wsystem(usb_power_off_cmd));
        }

        void cyclePower(powerCycleType_t powerCycleType)
        {
            LOG(INFO) << "Cycling power";

            if (powerStatus)
            {
                powerOff();
                powerStatus = false;
            }
            else
            {
                LOG(INFO) << "Power already turned off, skipping";
            }

            powerOn();

            if (powerCycleType == powerCycleType_t::STANDARD_WITH_DEVICE_CHECK)
            {
                LOG(INFO) << "Checking for device availability";

                // ensure the device is present
                auto startTime = test::millis();

                while (!_helper.devicePresent(true))
                {
                    if ((test::millis() - startTime) > max_connect_delay_ms)
                    {
                        LOG(ERROR) << "Device didn't connect in " << max_connect_delay_ms << " ms.";
                        FAIL();
                    }
                }
            }
        }

        void handshake()
        {
            LOG(INFO) << "Ensuring the device is available for handshake request...";
            auto startTime = test::millis();

            while (!_helper.devicePresent(true))
            {
                if ((test::millis() - startTime) > max_connect_delay_ms)
                {
                    LOG(ERROR) << "Device not available - waited " << max_connect_delay_ms << " ms.";
                    FAIL();
                }
            }

            LOG(INFO) << "Device available, attempting to send handshake";

            startTime = test::millis();

            while (handshake_ack != _helper.sendRawSysEx(handshake_req))
            {
                if ((test::millis() - startTime) > max_connect_delay_ms)
                {
                    LOG(ERROR) << "Device didn't respond to handshake in " << max_connect_delay_ms << " ms.";
                    FAIL();
                }

                LOG(ERROR) << "OpenDeck device not responding to handshake - trying again in " << handshake_retry_delay_ms << " ms";
                test::sleepMs(2000);
            }
        }

        void reboot(softRebootType_t type = softRebootType_t::STANDARD)
        {
            handshake();

            const std::string* cmd;

            switch (type)
            {
            case softRebootType_t::STANDARD:
            {
                LOG(INFO) << "Sending reboot request to the device";
                cmd = &reboot_req;
            }
            break;

            case softRebootType_t::FACTORY_RESET:
            {
                LOG(INFO) << "Sending factory reset request to the device";
                cmd = &factory_reset_req;
            }
            break;

            case softRebootType_t::BOOTLOADER:
            {
                LOG(INFO) << "Sending bootloader request to the device";
                cmd = &btldr_req;
            }
            break;

            default:
                return;
            }

            ASSERT_EQ(std::string(""), _helper.sendRawSysEx(*cmd, false));

            auto startTime = test::millis();

            LOG(INFO) << "Request sent. Waiting for the device to disconnect.";

            while (_helper.devicePresent(true))
            {
                if ((test::millis() - startTime) > max_disconnect_delay_ms)
                {
                    LOG(ERROR) << "Device didn't disconnect in " << max_disconnect_delay_ms << " ms.";
                    FAIL();
                }
            }

            LOG(INFO) << "Device disconnected.";

            if (type != softRebootType_t::BOOTLOADER)
            {
                handshake();
                _helper.flush();
            }
            else
            {
                auto startTime = test::millis();

                while (!_helper.devicePresent(true, true))
                {
                    if ((test::millis() - startTime) > max_connect_delay_ms)
                    {
                        LOG(ERROR) << "Device not available - waited " << max_connect_delay_ms << " ms.";
                        FAIL();
                    }
                }
            }
        }

        void factoryReset()
        {
            reboot(softRebootType_t::FACTORY_RESET);
        }

        void bootloader()
        {
            reboot(softRebootType_t::BOOTLOADER);
        }

        void flash(flashType_t flashType = flashType_t::DEVELOPMENT)
        {
            auto flash = [&](std::string target, std::string args)
            {
                const size_t ALLOWED_REPEATS = 2;
                int          result          = -1;
                std::string  flashTarget     = " TARGET=" + target;
                std::string  extraArgs       = "";

                if (flashType == flashType_t::RELEASE)
                {
                    LOG(INFO) << "Flashing release binary";
                    extraArgs += " FLASH_BINARY_DIR=" + latest_github_release_dir + " ";
                }
                else
                {
                    LOG(INFO) << "Flashing development binary";
                }

                for (size_t i = 0; i < ALLOWED_REPEATS; i++)
                {
                    LOG(INFO) << "Flashing the device, attempt " << i + 1;
                    result = test::wsystem(flash_cmd + flashTarget + " " + args + extraArgs);

                    if (result)
                    {
                        LOG(ERROR) << "Flashing failed";
                        cyclePower(powerCycleType_t::STANDARD);
                    }
                    else
                    {
                        LOG(INFO) << "Flashing successful";
                        break;
                    }
                }

                return result;
            };

#ifndef HW_SUPPORT_USB
            LOG(INFO) << "Flashing USB Link MCU";
            flash(std::string(USB_LINK_TARGET), std::string(FLASH_ARGS_USB_LINK));
#endif

            ASSERT_EQ(0, flash(std::string(BOARD_STRING), std::string(FLASH_ARGS)));
            cyclePower(powerCycleType_t::STANDARD_WITH_DEVICE_CHECK);
        }

        size_t cleanupMIDIResponse(std::string& response)
        {
            // drop empty lines, remove sysex, place each new channel midi message in new line
            std::string cleanupFile = "clean_response";
            test::wsystem("cat " + temp_midi_data_location + " | xargs | sed 's/F7 /F7\\n/g' | sed 's/F0/\\nF0/g' | grep -v '^F0' | xargs | sed 's/ /&\\n/3;P;D' > " + cleanupFile, response);
            test::wsystem("grep -c . " + cleanupFile, response);
            const auto receivedMessages = stoi(response);
            test::wsystem("cat " + cleanupFile, response);
            test::wsystem("rm " + cleanupFile + " " + temp_midi_data_location);

            return receivedMessages;
        };

        TestDatabase _database;
        MIDIHelper   _helper = MIDIHelper(true);

        const std::string handshake_req             = "F0 00 53 43 00 00 01 F7";
        const std::string handshake_ack             = "F0 00 53 43 01 00 01 F7";
        const std::string reboot_req                = "F0 00 53 43 00 00 7F F7";
        const std::string factory_reset_req         = "F0 00 53 43 00 00 44 F7";
        const std::string btldr_req                 = "F0 00 53 43 00 00 55 F7";
        const std::string backup_req                = "F0 00 53 43 00 00 1B F7";
        const std::string usb_power_off_cmd         = "echo write_high 1 > /dev/actl";
        const std::string usb_power_on_cmd          = "echo write_low 1 > /dev/actl";
        const uint32_t    turn_on_delay_ms          = 10000;
        const uint32_t    max_connect_delay_ms      = 25000;
        const uint32_t    max_disconnect_delay_ms   = 3000;
        const uint32_t    handshake_retry_delay_ms  = 2000;
        const std::string fw_build_dir              = "../src/build/";
        const std::string fw_build_type_subdir      = "release/";
        const std::string temp_midi_data_location   = "/tmp/temp_midi_data";
        const std::string backup_file_location      = "/tmp/backup.txt";
        const std::string latest_github_release_dir = "/tmp/latest_github_release";
        bool              powerStatus               = false;
    };
}    // namespace

TEST_F(HWTest, DatabaseInitialValues)
{
    constexpr size_t PARAM_SKIP = 2;

    // check only first and the last preset
    for (int preset = 0; preset < _database._instance.getSupportedPresets(); preset += (_database._instance.getSupportedPresets() - 1))
    {
        LOG(INFO) << "Checking initial values for preset " << preset + 1;
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::PRESETS, 0, preset));
        ASSERT_EQ(preset, _helper.readFromSystem(sys::Config::Section::global_t::PRESETS, 0));

        // MIDI block
        //----------------------------------
        // settings section
        // all values should be set to 0 except for the global channel which should be 1
        for (size_t i = 0; i < static_cast<uint8_t>(protocol::MIDI::setting_t::AMOUNT); i++)
        {
            switch (i)
            {
#ifdef HW_SUPPORT_DIN_MIDI
            case static_cast<size_t>(protocol::MIDI::setting_t::DIN_ENABLED):
            case static_cast<size_t>(protocol::MIDI::setting_t::DIN_THRU_USB):
            case static_cast<size_t>(protocol::MIDI::setting_t::USB_THRU_DIN):
            case static_cast<size_t>(protocol::MIDI::setting_t::DIN_THRU_DIN):
#endif

#ifdef HW_SUPPORT_BLE
            case static_cast<size_t>(protocol::MIDI::setting_t::BLE_ENABLED):
            case static_cast<size_t>(protocol::MIDI::setting_t::BLE_THRU_USB):
            case static_cast<size_t>(protocol::MIDI::setting_t::BLE_THRU_BLE):
            case static_cast<size_t>(protocol::MIDI::setting_t::USB_THRU_BLE):
#endif

#if defined(HW_SUPPORT_DIN_MIDI) && defined(HW_SUPPORT_BLE)
            case static_cast<size_t>(protocol::MIDI::setting_t::DIN_THRU_BLE):
            case static_cast<size_t>(protocol::MIDI::setting_t::BLE_THRU_DIN):
#endif

            case static_cast<size_t>(protocol::MIDI::setting_t::STANDARD_NOTE_OFF):
            case static_cast<size_t>(protocol::MIDI::setting_t::RUNNING_STATUS):
            case static_cast<size_t>(protocol::MIDI::setting_t::USB_THRU_USB):
            case static_cast<size_t>(protocol::MIDI::setting_t::USE_GLOBAL_CHANNEL):
            {
                ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::global_t::MIDI_SETTINGS, i));
            }
            break;

            case static_cast<size_t>(protocol::MIDI::setting_t::GLOBAL_CHANNEL):
            {
                ASSERT_EQ(1, _helper.readFromSystem(sys::Config::Section::global_t::MIDI_SETTINGS, i));
            }
            break;

            default:
                break;
            }
        }

        // button block
        //----------------------------------
        // type section
        // all values should be set to 0 (default type)
        for (size_t i = 0; i < io::Buttons::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::button_t::TYPE, i));
        }

        // midi message section
        // all values should be set to 0 (default/note)
        for (size_t i = 0; i < io::Buttons::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::button_t::MESSAGE_TYPE, i));
        }

        // midi id section
        // incremental values - first value should be 0, each successive value should be incremented by 1 for each group
        for (size_t group = 0; group < io::Buttons::Collection::GROUPS(); group++)
        {
            for (size_t i = 0; i < io::Buttons::Collection::SIZE(group); i += PARAM_SKIP)
            {
                ASSERT_EQ(i, _helper.readFromSystem(sys::Config::Section::button_t::MIDI_ID, i + io::Buttons::Collection::START_INDEX(group)));
            }
        }

        // midi velocity section
        // all values should be set to 127
        for (size_t i = 0; i < io::Buttons::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(127, _helper.readFromSystem(sys::Config::Section::button_t::VALUE, i));
        }

        // midi channel section
        // all values should be set to 1
        for (size_t i = 0; i < io::Buttons::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(1, _helper.readFromSystem(sys::Config::Section::button_t::CHANNEL, i));
        }

        // encoders block
        //----------------------------------
        // enable section
        // all values should be set to 0
        for (size_t i = 0; i < io::Encoders::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::encoder_t::ENABLE, i));
        }

        // invert section
        // all values should be set to 0
        for (size_t i = 0; i < io::Encoders::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::encoder_t::INVERT, i));
        }

        // mode section
        // all values should be set to 0
        for (size_t i = 0; i < io::Encoders::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::encoder_t::MODE, i));
        }

        // midi id section
        for (size_t i = 0; i < io::Encoders::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(i, _helper.readFromSystem(sys::Config::Section::encoder_t::MIDI_ID, i));
        }

        // midi channel section
        // all values should be set to 1
        for (size_t i = 0; i < io::Encoders::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(1, _helper.readFromSystem(sys::Config::Section::encoder_t::CHANNEL, i));
        }

        // pulses per step section
        // all values should be set to 4
        for (size_t i = 0; i < io::Encoders::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(4, _helper.readFromSystem(sys::Config::Section::encoder_t::PULSES_PER_STEP, i));
        }

        // analog block
        //----------------------------------
        // enable section
        // all values should be set to 0
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::analog_t::ENABLE, i));
        }

        // invert section
        // all values should be set to 0
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::analog_t::INVERT, i));
        }

        // type section
        // all values should be set to 0
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::analog_t::INVERT, i));
        }

        // midi id section
        // incremental values - first value should be 0, each successive value should be incremented by 1 for each group
        for (size_t group = 0; group < io::Analog::Collection::GROUPS(); group++)
        {
            for (size_t i = 0; i < io::Analog::Collection::SIZE(group); i += PARAM_SKIP)
            {
                ASSERT_EQ(i, _helper.readFromSystem(sys::Config::Section::analog_t::MIDI_ID, i + io::Analog::Collection::START_INDEX(group)));
            }
        }

        // lower limit section
        // all values should be set to 0
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::analog_t::LOWER_LIMIT, i));
        }

        // upper limit section
        // all values should be set to 16383
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(16383, _helper.readFromSystem(sys::Config::Section::analog_t::UPPER_LIMIT, i));
        }

        // midi channel section
        // all values should be set to 1
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(1, _helper.readFromSystem(sys::Config::Section::analog_t::CHANNEL, i));
        }

        // lower offset section
        // all values should be set to 0
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::analog_t::LOWER_OFFSET, i));
        }

        // upper offset section
        // all values should be set to 0
        for (size_t i = 0; i < io::Analog::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::analog_t::UPPER_OFFSET, i));
        }

        // LED block
        //----------------------------------
        // global section
        // all values should be set to 0
        for (size_t i = 0; i < static_cast<uint8_t>(io::LEDs::setting_t::AMOUNT); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::leds_t::GLOBAL, i));
        }

        // activation id section
        // incremental values - first value should be 0, each successive value should be incremented by 1 for each group
        for (size_t group = 0; group < io::LEDs::Collection::GROUPS(); group++)
        {
            for (size_t i = 0; i < io::LEDs::Collection::SIZE(group); i += PARAM_SKIP)
            {
                ASSERT_EQ(i, _helper.readFromSystem(sys::Config::Section::leds_t::ACTIVATION_ID, i + io::LEDs::Collection::START_INDEX(group)));
            }
        }

        // rgb enable section
        // all values should be set to 0
        for (size_t i = 0; i < io::LEDs::Collection::SIZE() / 3 + (io::Touchscreen::Collection::SIZE() / 3); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::leds_t::RGB_ENABLE, i));
        }

        // control type section
        // all values should be set to MIDI_IN_NOTE_MULTI_VAL
        for (size_t i = 0; i < io::LEDs::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(static_cast<uint32_t>(io::LEDs::controlType_t::MIDI_IN_NOTE_MULTI_VAL), _helper.readFromSystem(sys::Config::Section::leds_t::CONTROL_TYPE, i));
        }

        // activation value section
        // all values should be set to 127
        for (size_t i = 0; i < io::LEDs::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(127, _helper.readFromSystem(sys::Config::Section::leds_t::ACTIVATION_VALUE, i));
        }

        // midi channel section
        // all values should be set to 1
        for (size_t i = 0; i < io::LEDs::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(1, _helper.readFromSystem(sys::Config::Section::leds_t::CHANNEL, i));
        }

#ifdef HW_SUPPORT_I2C
        // i2c block
        //----------------------------------
        // display section
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::ENABLE)));
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::DEVICE_INFO_MSG)));
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::CONTROLLER)));
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::RESOLUTION)));
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::EVENT_TIME)));
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::MIDI_NOTES_ALTERNATE)));
        ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::i2c_t::DISPLAY, static_cast<size_t>(io::Display::setting_t::OCTAVE_NORMALIZATION)));
#endif

#ifdef HW_SUPPORT_TOUCHSCREEN
        // touchscreen block
        //----------------------------------
        // setting section
        // all values should be set to 0
        for (size_t i = 0; i < static_cast<uint8_t>(io::Touchscreen::setting_t::AMOUNT); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::SETTING, i));
        }

        // x position section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::X_POS, i));
        }

        // y position section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::Y_POS, i));
        }

        // WIDTH section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::WIDTH, i));
        }

        // HEIGHT section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::HEIGHT, i));
        }

        // on screen section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::ON_SCREEN, i));
        }

        // off screen section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::OFF_SCREEN, i));
        }

        // page switch enabled section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::PAGE_SWITCH_ENABLED, i));
        }

        // page switch index section
        // all values should be set to 0
        for (size_t i = 0; i < io::Touchscreen::Collection::SIZE(); i += PARAM_SKIP)
        {
            ASSERT_EQ(0, _helper.readFromSystem(sys::Config::Section::touchscreen_t::PAGE_SWITCH_INDEX, i));
        }
#endif
    }
}

#ifdef HW_SUPPORT_BOOTLOADER
TEST_F(HWTest, FwUpdate)
{
    LOG(INFO) << "Setting few random values in each available preset";

    for (int preset = 0; preset < _database._instance.getSupportedPresets(); preset++)
    {
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET), preset));
        ASSERT_EQ(preset, _helper.readFromSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET)));

        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::analog_t::MIDI_ID, 4, 15 + preset));
#ifdef ENCODERS_SUPPORTED
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::encoder_t::CHANNEL, 1, 2 + preset));
#endif
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::button_t::VALUE, 0, 90 + preset));

        ASSERT_EQ(15 + preset, _helper.readFromSystem(sys::Config::Section::analog_t::MIDI_ID, 4));
#ifdef ENCODERS_SUPPORTED
        ASSERT_EQ(2 + preset, _helper.readFromSystem(sys::Config::Section::encoder_t::CHANNEL, 1));
#endif
        ASSERT_EQ(90 + preset, _helper.readFromSystem(sys::Config::Section::button_t::VALUE, 0));
    }

    bootloader();

    std::string cmd = flash_cmd + std::string(" FLASH_TOOL=opendeck TARGET=") + std::string(BOARD_STRING);
    ASSERT_EQ(0, test::wsystem(cmd));
    LOG(INFO) << "Firmware file sent successfully, verifying that device responds to handshake";

    handshake();

    LOG(INFO) << "Verifying that the custom values are still active after FW update";

    for (int preset = 0; preset < _database._instance.getSupportedPresets(); preset++)
    {
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET), preset));
        ASSERT_EQ(preset, _helper.readFromSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET)));

        ASSERT_EQ(15 + preset, _helper.readFromSystem(sys::Config::Section::analog_t::MIDI_ID, 4));
#ifdef ENCODERS_SUPPORTED
        ASSERT_EQ(2 + preset, _helper.readFromSystem(sys::Config::Section::encoder_t::CHANNEL, 1));
#endif
        ASSERT_EQ(90 + preset, _helper.readFromSystem(sys::Config::Section::button_t::VALUE, 0));
    }
}

TEST_F(HWTest, FwUpdateFromLastRelease)
{
    flash(flashType_t::RELEASE);
    bootloader();

    std::string cmd = flash_cmd + std::string(" FLASH_TOOL=opendeck TARGET=") + std::string(BOARD_STRING);
    ASSERT_EQ(0, test::wsystem(cmd));
    LOG(INFO) << "Firmware file sent successfully, verifying that device responds to handshake";

    handshake();
}
#endif

TEST_F(HWTest, BackupAndRestore)
{
    LOG(INFO) << "Setting few random values in each available preset";

    for (int preset = 0; preset < _database._instance.getSupportedPresets(); preset++)
    {
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET), preset));
        ASSERT_EQ(preset, _helper.readFromSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET)));

        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::analog_t::MIDI_ID, 4, 15 + preset));
#ifdef ENCODERS_SUPPORTED
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::encoder_t::CHANNEL, 1, 2 + preset));
#endif
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::button_t::VALUE, 0, 90 + preset));

        ASSERT_EQ(15 + preset, _helper.readFromSystem(sys::Config::Section::analog_t::MIDI_ID, 4));
#ifdef ENCODERS_SUPPORTED
        ASSERT_EQ(2 + preset, _helper.readFromSystem(sys::Config::Section::encoder_t::CHANNEL, 1));
#endif
        ASSERT_EQ(90 + preset, _helper.readFromSystem(sys::Config::Section::button_t::VALUE, 0));
    }

    LOG(INFO) << "Sending backup request";
    std::string cmd = std::string("amidi -p ") + _helper.amidiPort(OPENDECK_MIDI_DEVICE_NAME) + " -S \"" + backup_req + "\" -d -t 4 > " + backup_file_location;
    ASSERT_EQ(0, test::wsystem(cmd));

    factoryReset();

    LOG(INFO) << "Verifying that the default values are active again";

    for (int preset = 0; preset < _database._instance.getSupportedPresets(); preset++)
    {
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET), preset));
        ASSERT_EQ(preset, _helper.readFromSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET)));

        ASSERT_EQ(4, _helper.readFromSystem(sys::Config::Section::analog_t::MIDI_ID, 4));
#ifdef ENCODERS_SUPPORTED
        ASSERT_EQ(1, _helper.readFromSystem(sys::Config::Section::encoder_t::CHANNEL, 1));
#endif
        ASSERT_EQ(127, _helper.readFromSystem(sys::Config::Section::button_t::VALUE, 0));
    }

    LOG(INFO) << "Restoring backup";

    // remove everything before the restore start indicator
    ASSERT_EQ(0, test::wsystem("sed -n -i '/^F0 00 53 43 00 00 1C F7$/,$p' " + backup_file_location));

    //...and also after the restore end indicator
    ASSERT_EQ(0, test::wsystem("sed -i '/^F0 00 53 43 00 00 1D F7$/q' " + backup_file_location));

    // send backup
    std::ifstream backupStream(backup_file_location);
    std::string   line;

    while (getline(backupStream, line))
    {
        // restore end indicator will reboot the board
        if (line.find("F0 00 53 43 00 00 1D F7") == std::string::npos)
        {
            ASSERT_NE(_helper.sendRawSysEx(line), std::string(""));
        }
        else
        {
            _helper.sendRawSysEx(line, false);
        }
    }

    LOG(INFO) << "Backup file sent successfully";

    handshake();

    LOG(INFO) << "Verifying that the custom values are active again";

    for (int preset = 0; preset < _database._instance.getSupportedPresets(); preset++)
    {
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET), preset));
        ASSERT_EQ(preset, _helper.readFromSystem(sys::Config::Section::global_t::PRESETS, static_cast<int>(database::Config::presetSetting_t::ACTIVE_PRESET)));

        ASSERT_EQ(15 + preset, _helper.readFromSystem(sys::Config::Section::analog_t::MIDI_ID, 4));
#ifdef ENCODERS_SUPPORTED
        ASSERT_EQ(2 + preset, _helper.readFromSystem(sys::Config::Section::encoder_t::CHANNEL, 1));
#endif
        ASSERT_EQ(90 + preset, _helper.readFromSystem(sys::Config::Section::button_t::VALUE, 0));
    }
}

TEST_F(HWTest, USBMIDIData)
{
    std::string cmd;
    std::string response;

    auto changePreset = [&]()
    {
        LOG(INFO) << "Switching preset";
        cmd = std::string("amidi -p ") + _helper.amidiPort(OPENDECK_MIDI_DEVICE_NAME) + " -S \"F0 00 53 43 00 00 01 00 00 02 00 00 00 00 F7\" -d -t 3 > " + temp_midi_data_location;
        ASSERT_EQ(0, test::wsystem(cmd, response));
    };

    changePreset();
    auto receivedMessages = cleanupMIDIResponse(response);
    LOG(INFO) << "Received " << receivedMessages << " USB messages after preset change:\n"
              << response;
    ASSERT_EQ(io::Buttons::Collection::SIZE(io::Buttons::GROUP_DIGITAL_INPUTS), receivedMessages);
}

#ifdef HW_SUPPORT_DIN_MIDI
#ifdef TEST_DIN_MIDI
TEST_F(HWTest, DINMIDIData)
{
    std::string cmd;
    std::string response;

    auto changePreset = [&]()
    {
        LOG(INFO) << "Switching preset";
        cmd = std::string("amidi -p ") + _helper.amidiPort(OPENDECK_MIDI_DEVICE_NAME) + " -S \"F0 00 53 43 00 00 01 00 00 02 00 00 00 00 F7\" -d -t 3";
        ASSERT_EQ(0, test::wsystem(cmd, response));
    };

    auto monitor = [&](bool usbMonitoring = false)
    {
        LOG(INFO) << "Monitoring DIN MIDI interface " << OUT_DIN_MIDI_PORT;
        cmd = std::string("amidi -p ") + _helper.amidiPort(OUT_DIN_MIDI_PORT) + " -d > " + temp_midi_data_location + " &";
        ASSERT_EQ(0, test::wsystem(cmd));

        if (usbMonitoring)
        {
            cmd = std::string("amidi -p ") + _helper.amidiPort(OPENDECK_MIDI_DEVICE_NAME) + " -d & ";
            ASSERT_EQ(0, test::wsystem(cmd));
        }
    };

    auto stopMonitoring = []()
    {
        test::wsystem("killall amidi > /dev/null");
    };

    monitor();
    changePreset();
    stopMonitoring();
    auto receivedMessages = cleanupMIDIResponse(response);

    LOG(INFO) << "Verifying that no data reached DIN MIDI interface due to the DIN MIDI being disabled";
    ASSERT_EQ(0, receivedMessages);

    LOG(INFO) << "Enabling DIN MIDI";
    ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::MIDI_SETTINGS, protocol::MIDI::setting_t::DIN_ENABLED, 1));
    monitor();
    changePreset();
    stopMonitoring();
    receivedMessages = cleanupMIDIResponse(response);
    LOG(INFO) << "Received " << receivedMessages << " DIN MIDI messages after preset change:\n"
              << response;
    ASSERT_EQ(io::Buttons::Collection::SIZE(io::Buttons::GROUP_DIGITAL_INPUTS), receivedMessages);

    // enable DIN MIDI passthrough, send data to DIN MIDI in to device and expect the same message passed to output port
    LOG(INFO) << "Enabling DIN to DIN thru";
    ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::MIDI_SETTINGS, protocol::MIDI::setting_t::DIN_THRU_DIN, 1));

    // having DIN to USB thru activated should not influence din to din thruing
    LOG(INFO) << "Enabling DIN to USB thru";
    ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::global_t::MIDI_SETTINGS, protocol::MIDI::setting_t::DIN_THRU_USB, 1));

    // monitor usb as well so that outgoing usb messages aren't "stuck"
    monitor(true);
    std::string  msg              = "90 00 7F";
    const size_t MESSAGES_TO_SEND = 100;

    LOG(INFO) << "Sending data to DIN MIDI interface " << IN_DIN_MIDI_PORT;
    LOG(INFO) << "Message: " << msg;

    cmd = "amidi -p " + _helper.amidiPort(IN_DIN_MIDI_PORT) + " -S \"" + msg + "\"";

    for (size_t i = 0; i < MESSAGES_TO_SEND; i++)
    {
        ASSERT_EQ(0, test::wsystem(cmd));
    }

    test::sleepMs(1000);
    stopMonitoring();
    receivedMessages = cleanupMIDIResponse(response);
    LOG(INFO) << "Received " << receivedMessages << " DIN MIDI messages on passthrough interface:\n"
              << response;
    ASSERT_EQ(MESSAGES_TO_SEND, receivedMessages);
    ASSERT_NE(response.find(msg), std::string::npos);
}
#endif
#endif

#ifdef TEST_IO
TEST_F(HWTest, InputOutput)
{
    std::string response;

    auto monitor = [&]()
    {
        LOG(INFO) << "Monitoring USB MIDI interface";
        auto cmd = std::string("amidi -p ") + _helper.amidiPort(OPENDECK_MIDI_DEVICE_NAME) + " -d > " + temp_midi_data_location + " &";
        ASSERT_EQ(0, test::wsystem(cmd));
    };

    auto stopMonitoring = []()
    {
        test::wsystem("killall amidi > /dev/null");
    };

#ifdef TEST_IO_ANALOG
    for (size_t i = 0; i < hwTestAnalogDescriptor.size(); i++)
    {
        LOG(INFO) << "Enabling analog input " << hwTestAnalogDescriptor.at(i).index;
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::analog_t::ENABLE, hwTestAnalogDescriptor.at(i).index, 1));
    }
#endif

    monitor();

#ifdef TEST_IO_SWITCHES
    for (size_t i = 0; i < hwTestSwDescriptor.size(); i++)
    {
        LOG(INFO) << "Toggling switch " << hwTestSwDescriptor.at(i).index;

        test::wsystem(std::string("echo write_high ") + std::to_string(hwTestSwDescriptor.at(i).pin) + " > /dev/actl");
        test::sleepMs(1000);
        test::wsystem(std::string("echo write_low ") + std::to_string(hwTestSwDescriptor.at(i).pin) + " > /dev/actl");
        test::sleepMs(1000);
    }
#endif

#ifdef TEST_IO_ANALOG
    for (size_t i = 0; i < hwTestAnalogDescriptor.size(); i++)
    {
        LOG(INFO) << "Toggling analog input " << hwTestAnalogDescriptor.at(i).index;

        test::wsystem(std::string("echo write_high ") + std::to_string(hwTestAnalogDescriptor.at(i).pin) + " > /dev/actl");
        test::sleepMs(1000);
        test::wsystem(std::string("echo write_low ") + std::to_string(hwTestAnalogDescriptor.at(i).pin) + " > /dev/actl");
        test::sleepMs(1000);
    }
#endif

    stopMonitoring();

    LOG(INFO) << "Printing response";
    test::wsystem("cat " + temp_midi_data_location + " | grep -v '^F0'");

    LOG(INFO) << "Verifying if received messages are valid";

#ifdef TEST_IO_SWITCHES
    for (size_t i = 0; i < hwTestSwDescriptor.size(); i++)
    {
        std::stringstream sstream;
        sstream << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << hwTestSwDescriptor.at(i).index;

        std::string msgOn  = "90 " + sstream.str() + " 00";
        std::string msgOff = "90 " + sstream.str() + " 7F";

        LOG(INFO) << "Searching for: " << msgOn;
        ASSERT_EQ(0, test::wsystem("cat " + temp_midi_data_location + " | grep \"" + msgOn + "\"", response));

        LOG(INFO) << "Searching for: " << msgOff;
        ASSERT_EQ(0, test::wsystem("cat " + temp_midi_data_location + " | grep \"" + msgOff + "\"", response));
    }
#endif

#ifdef TEST_IO_ANALOG
    for (size_t i = 0; i < hwTestAnalogDescriptor.size(); i++)
    {
        std::stringstream sstream;
        sstream << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << hwTestAnalogDescriptor.at(i).index;

        std::string msgOn  = "B0 " + sstream.str() + " 00";
        std::string msgOff = "B0 " + sstream.str() + " 7F";

        LOG(INFO) << "Searching for: " << msgOn;
        ASSERT_EQ(0, test::wsystem("cat " + temp_midi_data_location + " | grep \"" + msgOn + "\"", response));

        LOG(INFO) << "Searching for: " << msgOff;
        ASSERT_EQ(0, test::wsystem("cat " + temp_midi_data_location + " | grep \"" + msgOff + "\"", response));
    }
#endif

#ifdef TEST_IO_LEDS
    LOG(INFO) << "Checking LED output";

    for (size_t i = 0; i < hwTestLEDDescriptor.size(); i++)
    {
        LOG(INFO) << "Toggling LED " << hwTestLEDDescriptor.at(i).index << " on";
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::leds_t::TEST_COLOR, hwTestLEDDescriptor.at(i).index, 1));
        test::sleepMs(1000);

        LOG(INFO) << "Verifying that the LED is turned on";
        std::string ledOn = "read " + std::to_string(hwTestLEDDescriptor.at(i).pin);

#ifdef HW_LEDS_EXT_INVERT
        ledOn += " ok: 0";
#else
        ledOn += " ok: 1";
#endif

        test::wsystem("echo read " + std::to_string(hwTestLEDDescriptor.at(i).pin) + " | ssterm /dev/actl > " + temp_midi_data_location + " &", response);
        test::sleepMs(1000);

        test::wsystem("killall ssterm", response);
        while (test::wsystem("pgrep ssterm", response) == 0)
            ;

        LOG(INFO) << "Printing response";
        test::wsystem("cat " + temp_midi_data_location);

        ASSERT_EQ(0, test::wsystem("cat " + temp_midi_data_location + " | grep \"" + ledOn + "\"", response));

        LOG(INFO) << "Toggling LED " << hwTestLEDDescriptor.at(i).index << " off";
        ASSERT_TRUE(_helper.writeToSystem(sys::Config::Section::leds_t::TEST_COLOR, hwTestLEDDescriptor.at(i).index, 0));
        test::sleepMs(1000);

        LOG(INFO) << "Verifying that the LED is turned off";
        std::string ledOff = "read " + std::to_string(hwTestLEDDescriptor.at(i).pin);

#ifdef HW_LEDS_EXT_INVERT
        ledOff += " ok: 1";
#else
        ledOff += " ok: 0";
#endif

        test::wsystem("echo read " + std::to_string(hwTestLEDDescriptor.at(i).pin) + " | ssterm /dev/actl > " + temp_midi_data_location + " &", response);
        test::sleepMs(1000);

        test::wsystem("killall ssterm", response);
        while (test::wsystem("pgrep ssterm", response) == 0)
            ;

        LOG(INFO) << "Printing response";
        test::wsystem("cat " + temp_midi_data_location);

        ASSERT_EQ(0, test::wsystem("cat " + temp_midi_data_location + " | grep \"" + ledOff + "\"", response));
    }
#endif
}
#endif

#endif
#endif