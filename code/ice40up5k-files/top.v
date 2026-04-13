module top (
    output wifi_tx,
    input  wifi_rx,
    output LED
);
    // Simple blinky on an LED port
    // Modify to match the real output pin for the LED
    // For many iCE40UP5K boards, LED is e.g. on RGB_R, RGB_G, RGB_B, or generic GPIO.

    wire clk;
    // SB_HFOSC is the internal 48 MHz high-frequency oscillator
    SB_HFOSC #(
        .CLKHF_DIV("0b10") // Divide by 4 -> 12 MHz
    ) u_hfosc (
        .CLKHFEN(1'b1),
        .CLKHFPU(1'b1),
        .CLKHF(clk)
    );

    // 24-bit counter to get a roughly 1Hz blink rate at 12MHz
    reg [23:0] counter = 0;
    
    always @(posedge clk) begin
        counter <= counter + 1;
    end

    assign LED = counter[23];

    // Tie off unused
    assign wifi_tx = 1'b1;

endmodule
