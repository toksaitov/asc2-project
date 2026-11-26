# Makes the pictures of the Readme: the diagrams, and the animated pictures
# of the showcase from a run of `tour` on the emulator.
REFTRACE=../asc2-tools/reftrace/asc_reftrace
RACH2_DIR=../asc2-rach2
KERNEL=$(RACH2_DIR)/build/rach2.bin
DISK=$(RACH2_DIR)/build/disk.img
TOUR_IN=../asc2-tests/kernel/tour.in
BUILD_DIR=build
TOUR=$(BUILD_DIR)/tour.film
FILM=python3 -I tools/film.py

.PHONY: all
all: diagrams films

.PHONY: diagrams
diagrams:
	python3 -I tools/diagrams.py .

# Every frame that reaches the screen while the kernel runs `tour` with
# nobody at the button
$(TOUR): $(KERNEL) $(DISK) $(TOUR_IN)
	@mkdir -p $(BUILD_DIR)
	$(REFTRACE) -q -r -n 100000000 -d $(DISK) -i $(TOUR_IN) -o $(BUILD_DIR)/tour.out -m $@ $(KERNEL)

# The seconds are those of the bridge's clock in that run
.PHONY: films
films: $(TOUR)
	$(FILM) gif $(TOUR) title.gif  472.6 481.0 -s 4 -e 4
	$(FILM) gif $(TOUR) shapes.gif 16.5  46.2
	$(FILM) gif $(TOUR) flight.gif 58.0  79.4
	$(FILM) gif $(TOUR) race.gif   92.5  123.0 -e 3
	$(FILM) gif $(TOUR) pilot.gif  142.0 187.0
	$(FILM) gif $(TOUR) fence.gif  200.5 230.0
	$(FILM) gif $(TOUR) scope.gif  244.5 267.5
	$(FILM) gif $(TOUR) guess.gif  280.0 315.0
	$(FILM) gif $(TOUR) sched.gif  330.5 354.4
	$(FILM) gif $(TOUR) mark.gif   372.0 398.0 -e 3
	$(FILM) gif $(TOUR) film.gif   423.2 472.4

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
