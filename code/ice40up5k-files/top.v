`default_nettype none

module top (
    output wire led
);
    wire clk;

    // Internal 48 MHz oscillator divided by 4 gives a 12 MHz fabric clock.
    SB_HFOSC #(
        .CLKHF_DIV("0b10")
    ) u_hfosc (
        .CLKHFEN(1'b1),
        .CLKHFPU(1'b1),
        .CLKHF(clk)
    );

    localparam integer CLK_HZ = 12_000_000;
    localparam integer HALF_PERIOD_TICKS = CLK_HZ / 2;

    reg [22:0] counter = 23'd0;
    reg led_on = 1'b0;

    always @(posedge clk) begin
        if (counter == HALF_PERIOD_TICKS - 1) begin
            counter <= 23'd0;
            led_on <= ~led_on;
        end else begin
            counter <= counter + 1'b1;
        end
    end

    // Board LED D1 is wired directly from +3V3 to RGB0/pin 39.
    // Use the UP5K constant-current RGB driver; there is no series resistor.
    SB_RGBA_DRV #(
        .CURRENT_MODE("0b1"),
        .RGB0_CURRENT("0b000011"),
        .RGB1_CURRENT("0b000000"),
        .RGB2_CURRENT("0b000000")
    ) u_rgb_drv (
        .CURREN(1'b1),
        .RGBLEDEN(1'b1),
        .RGB0PWM(led_on),
        .RGB1PWM(1'b0),
        .RGB2PWM(1'b0),
        .RGB0(led),
        .RGB1(),
        .RGB2()
    );

endmodule

`default_nettype wire
