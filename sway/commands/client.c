#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "util.h"

static void container_update_iterator(struct sway_container *con, void *data) {
	container_update(con);
}

static struct cmd_results *handle_colors(int argc, char **argv, char *cmd_name,
		struct border_colors *class, const char *default_indicator) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_AT_LEAST, 3)) ||
			(error = checkarg(argc, cmd_name, EXPECTED_AT_MOST, 5))) {
		return error;
	}

	if (argc > 3 && strcmp(cmd_name, "client.focused_tab_title") == 0) {
		sway_log(SWAY_ERROR,
				"Warning: indicator and child_border colors have no effect for %s",
				cmd_name);
	}

	struct border_colors colors = {0};
	const char *ind_hex = argc > 3 ? argv[3] : default_indicator;
	const char *child_hex = argc > 4 ? argv[4] : argv[1];  // def to background

	struct {
		const char *name;
		const char *hex;
		float *rgba[4];
	} properties[] = {
		{ "border", argv[0], colors.border },
		{ "background", argv[1], colors.background },
		{ "text", argv[2], colors.text },
		{ "indicator", ind_hex, colors.indicator },
		{ "child_border", child_hex, colors.child_border }
	};
	for (size_t i = 0; i < sizeof(properties) / sizeof(properties[0]); i++) {
		uint32_t color;
		if (!parse_color(properties[i].hex, &color)) {
			return cmd_results_new(CMD_INVALID, "Invalid %s color %s",
					properties[i].name, properties[i].hex);
		}
		color_to_rgba(*properties[i].rgba, color);
	}

	memcpy(class, &colors, sizeof(struct border_colors));
	return error;
}

static struct cmd_results *handle_command(int argc, char **argv, char *cmd_name,
		enum border_color_class class, const char *default_indicator) {
	struct cmd_results *error = NULL;
	if ((error = handle_colors(argc, argv, cmd_name, config->border_colors[class],
					default_indicator))) {
		return error;
	}

	if (config->active) {
		root_for_each_container(container_update_iterator, NULL);
	}

	return error;
}

static struct cmd_results *handle_container_command(
		int argc, char **argv, char *cmd_name,
		struct sway_container *con,
		enum border_color_class class,
		const char *default_indicator) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcmp(argv[0], "default") == 0) {
		if ((error = checkarg(argc, cmd_name, EXPECTED_AT_MOST, 1))) {
			return error;
		}

		/* free the per-container class configuration */
		if (con->border_colors[class]) {
			free(con->border_colors[class]);
			con->border_colors[class] = NULL;
		}
	} else {
		/* avoid needless reallocation to prevent heap fragmentation */
		struct border_colors *newclass = con->border_colors[class];
		if (!con->border_colors[class]) {
			newclass = calloc(1, sizeof(*newclass));
			if (!newclass) {
				return cmd_results_new(CMD_INVALID, "Unable "
						"to allocate color configuration");
			}
		}

		if ((error = handle_colors(argc, argv, cmd_name,
						newclass, default_indicator))) {
			free(newclass);
			return error;
		}

		if (!con->border_colors[class]) {
			con->border_colors[class] = newclass;
		}
	}

	if (config->active) {
		root_for_each_container(container_update_iterator, NULL);
	}

	return error;
}

static struct cmd_results *handle_command_context(int argc, char **argv, char *cmd_name,
		enum border_color_class class,
		struct sway_container *con,
		const char *default_indicator) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcmp(argv[0], "global") == 0) {
		error = handle_command(argc - 1, argv + 1, cmd_name,
				class, default_indicator);
	} else if (con) {
		error = handle_container_command(argc, argv, cmd_name,
				con, class, default_indicator);
	} else {
		error = handle_command(argc, argv, cmd_name,
				class, default_indicator);
	}

	return error ? error : cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_client_focused(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	return handle_command_context(argc, argv, "client.focused",
			border_color_class_focused, con, "#2e9ef4ff");
}

struct cmd_results *cmd_client_focused_inactive(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	return handle_command_context(argc, argv, "client.focused_inactive",
			border_color_class_focused_inactive, con, "#484e50ff");
}

struct cmd_results *cmd_client_unfocused(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	return handle_command_context(argc, argv, "client.unfocused",
			border_color_class_unfocused, con, "#292d2eff");
}

struct cmd_results *cmd_client_urgent(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	return handle_command_context(argc, argv, "client.urgent",
			border_color_class_urgent, con, "#900000ff");
}

struct cmd_results *cmd_client_noop(int argc, char **argv) {
	sway_log(SWAY_INFO, "Warning: %s is ignored by sway", argv[-1]);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_client_focused_tab_title(int argc, char **argv) {
	struct cmd_results *result =  handle_command(argc, argv,
			"client.focused_tab_title",
			border_color_class_focused_tab_title, "#2e9ef4ff");
	if (result && result->status == CMD_SUCCESS) {
		config->has_focused_tab_title = true;
	}
	return result;
}
