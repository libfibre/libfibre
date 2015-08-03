struct state_machine {
	int dummy;
};

static inline void state_machine_init(struct state_machine *p)
{
	p->dummy = 0;
}
void state_machine_update(struct state_machine *);
