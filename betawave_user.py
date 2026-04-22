"""User script hooks for betawave."""


def init(ctx):
    """Initialize monitors, plots, and optional VOFA settings."""
    # ctx.create_monitor("i")
    ctx.create_window(title="betawave", fullscreen=None)


def run(ctx):
    """Collect data, run custom math, and update plot/VOFA outputs."""
    # value_i = ctx.data["i"]
    # ctx.plot_symbol(value_i, "i")
    # ctx.send_vofa(value_i)
