IMAGE := xv6-dev
DOCKER_RUN := docker run --rm -it --name xv6 -v $(CURDIR):/xv6 -w /xv6 $(IMAGE)

.DEFAULT_GOAL := qemu
.PHONY: image

image:
	docker build -t $(IMAGE) .

gdb:
	docker exec -it xv6 gdb-multiarch -x .gdbinit

.DEFAULT:
	$(DOCKER_RUN) make -f Makefile $@