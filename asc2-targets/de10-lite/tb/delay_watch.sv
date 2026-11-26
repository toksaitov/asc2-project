// Watches one signal before its delay and after it, for the test of the
// board's top module, and counts the changes that come at a wrong time: at
// once with `is_late` low, and 10 to 15 ns later with it high. A signal that
// has changed twice within a delay, as in the test of the guard, has no one
// time, and its change is not counted.
module delay_watch #(
    parameter string NAME = ""
) (
    input logic is_late,
    input logic early,
    input logic late
);
    localparam realtime SHORTEST = 9.9ns;
    localparam realtime LONGEST  = 15.1ns;

    realtime     changed;
    realtime     changed_before;
    realtime     took;
    int unsigned changes;
    int unsigned errors;

    initial begin
        changed        = 0;
        changed_before = -LONGEST;
        forever begin
            @(early);
            changed_before = changed;
            changed        = $realtime;
        end
    end

    initial begin
        took    = 0;
        changes = 0;
        errors  = 0;
        forever begin
            @(late);
            took = $realtime - changed;
            if (!is_late || changed - changed_before > LONGEST) begin
                ++changes;
                if (is_late ? (took < SHORTEST || took > LONGEST) : (took != 0)) begin
                    $display("FAILED at %0t: %s comes after %0t", $realtime, NAME, took);
                    ++errors;
                end
            end
        end
    end
endmodule
