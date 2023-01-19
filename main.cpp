#include <cstddef>
#include <cstdint>

#include "mbed.h"
#include "spirit/include/A3921.h"
#include "spirit/include/FakeUdpConverter.h"
#include "spirit/include/Id.h"
#include "spirit/include/MdLed.h"
#include "spirit/include/PwmDataConverter.h"
#include "spirit/platform/mbed/include/DigitalOut.h"
#include "spirit/platform/mbed/include/PwmOut.h"

static constexpr uint32_t pc_baud    = 115'200;
static constexpr auto     loop_rate  = 1ms;
static constexpr uint32_t defalt_ttl = 1000;  // 1s

// main() runs in its own thread in the OS
int main()
{
    BufferedSerial pc(USBTX, USBRX, pc_baud);

    BusIn dip_sw(PB_3, PB_4, PB_5, PB_6);  // 下位ビット ~ 上位ビット

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

    CANMessage               msg;
    spirit::FakeUdpConverter fake_udp;
    spirit::PwmDataConverter pwm_data;

    int32_t ttl = -1;

    while (true) {
        // バッファに溜まっているデータを全て処理したいので、 while で回す

        // CANのfilterを使う場合、while文を以下のように書き換える
        // while (can.read(msg, filter_handle)) {
        while (can.read(msg)) {
            if (msg.id == can_id) {
                ttl = defalt_ttl;

                constexpr std::size_t max_payload_size = 64;
                uint8_t               payload[8]       = {};
                std::size_t           payload_size;
                fake_udp.decode(msg.data, msg.len * 8, max_payload_size, payload, payload_size);

                spirit::Motor motor;
                pwm_data.decode(payload, payload_size, motor);

                a3921.duty_cycle(motor.get_duty_cycle());
                a3921.state(motor.get_state());

                mdled.mode(spirit::MdLed::BlinkMode::Normal);
                mdled.state(motor.get_state());
            }
        }

        if (ttl > 0) {
            ttl--;
        } else if (ttl == 0) {
            // 一定時間データが来ないとき、安全のためモーターを停止させる
            a3921.state(spirit::Motor::State::Brake);
            mdled.mode(spirit::MdLed::BlinkMode::Concurrent);
        } else {
            // 一度も通信が来ていない場合は、ttlが負の値になる
            mdled.mode(spirit::MdLed::BlinkMode::Alternate);
        }

        ThisThread::sleep_for(loop_rate);
    }
}
