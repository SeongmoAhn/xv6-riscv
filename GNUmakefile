IMAGE := xv6-dev
DOCKER_RUN := docker run --rm -it -v $(CURDIR):/xv6 -w /xv6 $(IMAGE)

.DEFAULT_GOAL := qemu
.PHONY: image

image:
	docker build -t $(IMAGE) .

.DEFAULT:
	$(DOCKER_RUN) make -f Makefile $@