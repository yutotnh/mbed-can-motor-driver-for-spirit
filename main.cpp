#include <cstddef>
#include <cstdint>

#include "mbed.h"
#include "spirit.h"

static constexpr uint32_t pc_baud    = 115'200;
static constexpr auto     loop_rate  = 1ms;
static constexpr uint32_t defalt_ttl = 1000;  // 1s

void debug_float(float val);

// main() runs in its own thread in the OS
int main()
{
    BufferedSerial pc(USBTX, USBRX, pc_baud);

    BusIn dip_sw(PB_3, PB_4, PB_5, PB_6);  // 下位ビット ~ 上位ビット

    dip_sw.mode(PullDown);

    const uint32_t dip_sw_value = dip_sw.read();
    const uint32_t can_id       = spirit::can::get_motor_id(1, 0, dip_sw_value);

    CAN can(PA_11, PA_12);
    // CANのfilterを使う場合、以下のコメントを外す
    // const uint32_t filter_handle = can.filter(can_id, 0x7FF);

    printf("mbed-can-motor-driver-for-spirit v0.1.0\n");
    printf("spirit v0.1.0\n");
    printf("CAN ID: %3lX (DIW SW: %1lX)\n", can_id, dip_sw_value);

    spirit::mbed::DigitalOut sr(PB_0);
    spirit::mbed::PwmOut     pwmh(PA_6);
    spirit::mbed::PwmOut     pwml(PA_7);
    spirit::mbed::PwmOut     phase(PB_1);
    spirit::mbed::DigitalOut reset(PA_5);

    spirit::A3921 a3921(sr, pwmh, pwml, phase, reset);

    spirit::mbed::DigitalOut led0(PA_10);
    spirit::mbed::DigitalOut led1(PA_9);

    spirit::MdLed mdled(led0, led1);

    CANMessage                 msg;
    spirit::FakeUdpConverter   fake_udp;
    spirit::MotorDataConverter motor_data;

    spirit::mbed::InterruptIn a_phase(PA_0);
    spirit::mbed::InterruptIn b_phase(PA_1);

    spirit::SpeedController speed_controller(a_phase, b_phase);

    spirit::Motor::State last_state = spirit::Motor::State::Brake;

    int32_t ttl = -1;

    speed_controller.limit(0.99f, 0.00f);
    speed_controller.pid_gain(0.30f, 0.80f, 0.20f);

    while (true) {
        // バッファに溜まっているデータを全て処理したいので、 while で回す

        // CANのfilterを使う場合、while文を以下のように書き換える
        // while (can.read(msg, filter_handle)) {
        spirit::Motor motor;
        while (can.read(msg)) {
            if (msg.id == can_id) {
                ttl = defalt_ttl;

                constexpr std::size_t max_payload_size = 64;
                uint8_t               payload[8]       = {};
                std::size_t           payload_size;
                fake_udp.decode(msg.data, msg.len * 8, max_payload_size, payload, payload_size);

                if (motor_data.decode(payload, payload_size, motor)) {
                } else {
                    spirit::Error& error = spirit::Error::get_instance();
                    error.error(spirit::Error::Type::UnknownValue, 0, __FILE__, __func__, __LINE__, "Failed to decode");
                }
            }
        }

        if (ttl > 0) {
            ttl--;
            float                duty_cycle = 0.0f;
            spirit::Motor::State state      = spirit::Motor::State::Brake;
            switch (motor.get_control_system()) {
                case spirit::Motor::ControlSystem::PWM:
                    duty_cycle = motor.get_duty_cycle();
                    state      = motor.get_state();
                    break;
                case spirit::Motor::ControlSystem::Speed:
                    float speed;
                    speed = motor.get_speed();
                    state = motor.get_state();

                    float Kp, Ki, Kd;
                    motor.get_pid_gain_factor(Kp, Ki, Kd);  // Kd はデータが来ない

                    Kd = Ki / 4.0f;  // KdはKiの1/4にする(経験則)

                    if (speed_controller.pid_gain(Kp, Ki, Kd) || last_state != state) {
                        speed_controller.reset();
                        duty_cycle = 0.00f;
                    } else {
                        duty_cycle = speed_controller.calculation(speed, 0.001f);
                    }

                    debug("kp = ");
                    debug_float(kp);
                    debug(", ki = ");
                    debug_float(ki);
                    debug(", kd = ");
                    debug_float(kd);
                    debug(", target_rps = ");
                    debug_float(speed);
                    debug(", rps = ");
                    debug_float(speed_controller.rps());
                    debug(", duty = ");
                    debug_float(duty_cycle);
                    debug("\r\n");

                    break;
                default:
                    spirit::Error::get_instance().error(spirit::Error::Type::UnknownValue, 0, __FILE__, __func__,
                                                        __LINE__, "Unknown motor control system (%d)",
                                                        static_cast<uint32_t>(motor.get_control_system()));
                    break;
            }

            last_state = state;

            mdled.mode(spirit::MdLed::BlinkMode::Normal);
            mdled.state(state);

            a3921.duty_cycle(duty_cycle);
            a3921.state(state);
            a3921.run();
        } else if (ttl == 0) {
            // 一定時間データが来ないとき、安全のためモーターを停止させる
            a3921.state(spirit::Motor::State::Brake);
            a3921.run();
            mdled.mode(spirit::MdLed::BlinkMode::Concurrent);
        } else {
            // 一度も通信が来ていない場合は、ttlが負の値になる
            mdled.mode(spirit::MdLed::BlinkMode::Alternate);
        }

        mdled.coordinate();
        ThisThread::sleep_for(loop_rate);
    }
}

void debug_float(float val)
{
    debug("%d.%d", (int)val, (int)((val - (int)val) * 1000));
}