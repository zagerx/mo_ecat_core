/**
 * @file main.c
 * @brief mo_ecat 配置代码生成器
 *
 * 将 YAML 配置文件在构建期转换为 C 数据模型，目标程序运行时不依赖 YAML。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <yaml.h>

#define MAX_JOINTS 32
#define PATH_SIZE 4096

struct joint_entry {
    const char *name;
    const char *group_enum;
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    unsigned int mark_line;
    unsigned int mark_col;
};

static const char *prog_name;

static void print_error(const char *file, unsigned int line, unsigned int col,
                        const char *path, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "[ERROR] %s:%u:%u %s: ", file, line, col, path);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void print_file_error(const char *file, const char *reason)
{
    fprintf(stderr, "[ERROR] %s: %s\n", file, reason);
}

static int mkdir_for_file(const char *file_path)
{
    char dir[PATH_SIZE];
    const char *last_slash;

    last_slash = strrchr(file_path, '/');
    if (!last_slash) {
        return 0;
    }

    if ((size_t)(last_slash - file_path) >= sizeof(dir) - 1) {
        return -1;
    }

    memcpy(dir, file_path, (size_t)(last_slash - file_path));
    dir[last_slash - file_path] = '\0';

    for (char *p = dir + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            (void)mkdir(dir, 0755);
            *p = '/';
        }
    }

    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

static yaml_node_t *mapping_get(yaml_document_t *doc, yaml_node_t *map,
                                const char *key)
{
    yaml_node_pair_t *pair;

    if (!map || map->type != YAML_MAPPING_NODE) {
        return NULL;
    }

    for (pair = map->data.mapping.pairs.start;
         pair < map->data.mapping.pairs.top; ++pair) {
        yaml_node_t *k = yaml_document_get_node(doc, pair->key);
        if (k && k->type == YAML_SCALAR_NODE &&
            strcmp((const char *)k->data.scalar.value, key) == 0) {
            return yaml_document_get_node(doc, pair->value);
        }
    }

    return NULL;
}

static int expect_scalar(yaml_document_t *doc, yaml_node_t *node,
                         const char *file, const char *path,
                         const char **out_value)
{
    (void)doc;

    if (!node) {
        print_error(file, 1, 1, path, "missing required field");
        return -1;
    }

    if (node->type != YAML_SCALAR_NODE) {
        print_error(file, (unsigned int)(node->start_mark.line + 1),
                    (unsigned int)(node->start_mark.column + 1),
                    path, "expected scalar value");
        return -1;
    }

    *out_value = (const char *)node->data.scalar.value;
    return 0;
}

static int expect_mapping(yaml_document_t *doc, yaml_node_t *node,
                          const char *file, const char *path,
                          yaml_node_t **out_map)
{
    (void)doc;

    if (!node) {
        print_error(file, 1, 1, path, "missing required field");
        return -1;
    }

    if (node->type != YAML_MAPPING_NODE) {
        print_error(file, (unsigned int)(node->start_mark.line + 1),
                    (unsigned int)(node->start_mark.column + 1),
                    path, "expected mapping");
        return -1;
    }

    *out_map = node;
    return 0;
}

static int expect_sequence(yaml_document_t *doc, yaml_node_t *node,
                           const char *file, const char *path,
                           yaml_node_t **out_seq)
{
    (void)doc;

    if (!node) {
        print_error(file, 1, 1, path, "missing required field");
        return -1;
    }

    if (node->type != YAML_SEQUENCE_NODE) {
        print_error(file, (unsigned int)(node->start_mark.line + 1),
                    (unsigned int)(node->start_mark.column + 1),
                    path, "expected sequence");
        return -1;
    }

    *out_seq = node;
    return 0;
}

static int check_version(yaml_document_t *doc, yaml_node_t *root, const char *file)
{
    yaml_node_t *version_node;
    const char *version = NULL;

    if (!root || root->type != YAML_MAPPING_NODE) {
        print_error(file, 1, 1, "", "root must be a mapping");
        return -1;
    }

    version_node = mapping_get(doc, root, "version");
    if (!version_node || version_node->type != YAML_SCALAR_NODE) {
        print_error(file, 1, 1, "version", "missing required scalar 'version'");
        return -1;
    }

    version = (const char *)version_node->data.scalar.value;
    if (strcmp(version, "1.0") != 0) {
        print_error(file, (unsigned int)(version_node->start_mark.line + 1),
                    (unsigned int)(version_node->start_mark.column + 1),
                    "version", "unsupported version '%s' (expected: 1.0)", version);
        return -1;
    }

    return 0;
}

static int parse_uint(const char *str, unsigned long *out)
{
    char *end;
    unsigned long val;

    if (!str || str[0] == '\0') {
        return -1;
    }

    errno = 0;
    val = strtoul(str, &end, 0);
    if (errno != 0 || *end != '\0') {
        return -1;
    }

    *out = val;
    return 0;
}

static int parse_master_yaml(const char *file, yaml_document_t *doc,
                             char *interface, size_t interface_size)
{
    yaml_node_t *root;
    yaml_node_t *master_node;
    yaml_node_t *interface_node;
    const char *value;

    root = yaml_document_get_root_node(doc);
    if (check_version(doc, root, file) < 0) {
        return -1;
    }

    if (expect_mapping(doc, mapping_get(doc, root, "master"),
                       file, "master", &master_node) < 0) {
        return -1;
    }

    interface_node = mapping_get(doc, master_node, "interface");
    if (expect_scalar(doc, interface_node, file, "master.interface", &value) < 0) {
        return -1;
    }

    if (value[0] == '\0') {
        print_error(file, (unsigned int)(interface_node->start_mark.line + 1),
                    (unsigned int)(interface_node->start_mark.column + 1),
                    "master.interface", "must be non-empty");
        return -1;
    }

    if (strlen(value) >= interface_size) {
        print_error(file, (unsigned int)(interface_node->start_mark.line + 1),
                    (unsigned int)(interface_node->start_mark.column + 1),
                    "master.interface", "value too long");
        return -1;
    }

    strncpy(interface, value, interface_size - 1);
    interface[interface_size - 1] = '\0';
    return 0;
}

static const char *group_to_enum(const char *group)
{
    if (strcmp(group, "torso") == 0) {
        return "ROBOT_GROUP_TORSO";
    }
    if (strcmp(group, "left_arm") == 0) {
        return "ROBOT_GROUP_LEFT_ARM";
    }
    if (strcmp(group, "right_arm") == 0) {
        return "ROBOT_GROUP_RIGHT_ARM";
    }
    if (strcmp(group, "left_leg") == 0) {
        return "ROBOT_GROUP_LEFT_LEG";
    }
    if (strcmp(group, "right_leg") == 0) {
        return "ROBOT_GROUP_RIGHT_LEG";
    }
    if (strcmp(group, "head") == 0) {
        return "ROBOT_GROUP_HEAD";
    }
    return NULL;
}

static int parse_robot_yaml(const char *file, yaml_document_t *doc,
                            char *robot_name, size_t robot_name_size,
                            struct joint_entry *joints, size_t *joint_count)
{
    yaml_node_t *root;
    yaml_node_t *robot_node;
    yaml_node_t *name_node;
    yaml_node_t *joints_node;
    const char *name;
    yaml_node_item_t *item;

    root = yaml_document_get_root_node(doc);
    if (check_version(doc, root, file) < 0) {
        return -1;
    }

    if (expect_mapping(doc, mapping_get(doc, root, "robot"),
                       file, "robot", &robot_node) < 0) {
        return -1;
    }

    name_node = mapping_get(doc, robot_node, "name");
    if (expect_scalar(doc, name_node, file, "robot.name", &name) < 0) {
        return -1;
    }

    if (name[0] == '\0') {
        print_error(file, (unsigned int)(name_node->start_mark.line + 1),
                    (unsigned int)(name_node->start_mark.column + 1),
                    "robot.name", "must be non-empty");
        return -1;
    }

    if (strlen(name) >= robot_name_size) {
        print_error(file, (unsigned int)(name_node->start_mark.line + 1),
                    (unsigned int)(name_node->start_mark.column + 1),
                    "robot.name", "value too long");
        return -1;
    }

    strncpy(robot_name, name, robot_name_size - 1);
    robot_name[robot_name_size - 1] = '\0';

    if (expect_sequence(doc, mapping_get(doc, robot_node, "joints"),
                        file, "robot.joints", &joints_node) < 0) {
        return -1;
    }

    *joint_count = 0;
    for (item = joints_node->data.sequence.items.start;
         item < joints_node->data.sequence.items.top; ++item) {
        yaml_node_t *joint_node = yaml_document_get_node(doc, *item);
        yaml_node_t *identity_node;
        const char *joint_name;
        const char *group_str;
        const char *position_str;
        const char *vendor_str;
        const char *product_str;
        const char *group_enum;
        unsigned long position;
        unsigned long vendor_id;
        unsigned long product_code;
        struct joint_entry *joint;
        size_t i;

        if (*joint_count >= MAX_JOINTS) {
            print_error(file, (unsigned int)(joint_node->start_mark.line + 1),
                        (unsigned int)(joint_node->start_mark.column + 1),
                        "robot.joints", "exceeds maximum of %d joints", MAX_JOINTS);
            return -1;
        }

        if (!joint_node || joint_node->type != YAML_MAPPING_NODE) {
            print_error(file, (unsigned int)(joint_node->start_mark.line + 1),
                        (unsigned int)(joint_node->start_mark.column + 1),
                        "robot.joints[]", "expected mapping");
            return -1;
        }

        yaml_node_t *group_node = mapping_get(doc, joint_node, "group");
        yaml_node_t *name_node_field = mapping_get(doc, joint_node, "joint_name");

        if (expect_scalar(doc, name_node_field,
                          file, "robot.joints[].joint_name", &joint_name) < 0 ||
            expect_scalar(doc, group_node,
                          file, "robot.joints[].group", &group_str) < 0 ||
            expect_mapping(doc, mapping_get(doc, joint_node, "identity"),
                           file, "robot.joints[].identity", &identity_node) < 0) {
            return -1;
        }

        group_enum = group_to_enum(group_str);
        if (!group_enum) {
            print_error(file, (unsigned int)(group_node->start_mark.line + 1),
                        (unsigned int)(group_node->start_mark.column + 1),
                        "robot.joints[].group", "unknown group '%s'", group_str);
            return -1;
        }

        if (expect_scalar(doc, mapping_get(doc, identity_node, "position"),
                          file, "robot.joints[].identity.position", &position_str) < 0 ||
            expect_scalar(doc, mapping_get(doc, identity_node, "vendor_id"),
                          file, "robot.joints[].identity.vendor_id", &vendor_str) < 0 ||
            expect_scalar(doc, mapping_get(doc, identity_node, "product_code"),
                          file, "robot.joints[].identity.product_code", &product_str) < 0) {
            return -1;
        }

        if (parse_uint(position_str, &position) < 0 || position > UINT16_MAX) {
            print_error(file, (unsigned int)(mapping_get(doc, identity_node, "position")->start_mark.line + 1),
                        (unsigned int)(mapping_get(doc, identity_node, "position")->start_mark.column + 1),
                        "robot.joints[].identity.position", "invalid uint16 value '%s'", position_str);
            return -1;
        }

        if (parse_uint(vendor_str, &vendor_id) < 0 || vendor_id > UINT32_MAX) {
            print_error(file, (unsigned int)(mapping_get(doc, identity_node, "vendor_id")->start_mark.line + 1),
                        (unsigned int)(mapping_get(doc, identity_node, "vendor_id")->start_mark.column + 1),
                        "robot.joints[].identity.vendor_id", "invalid uint32 value '%s'", vendor_str);
            return -1;
        }

        if (parse_uint(product_str, &product_code) < 0 || product_code > UINT32_MAX) {
            print_error(file, (unsigned int)(mapping_get(doc, identity_node, "product_code")->start_mark.line + 1),
                        (unsigned int)(mapping_get(doc, identity_node, "product_code")->start_mark.column + 1),
                        "robot.joints[].identity.product_code", "invalid uint32 value '%s'", product_str);
            return -1;
        }

        for (i = 0; i < *joint_count; ++i) {
            if (strcmp(joints[i].name, joint_name) == 0) {
                print_error(file, (unsigned int)(joint_node->start_mark.line + 1),
                            (unsigned int)(joint_node->start_mark.column + 1),
                            "robot.joints[].joint_name",
                            "duplicate joint name '%s'", joint_name);
                return -1;
            }
            if (joints[i].position == (uint16_t)position &&
                joints[i].vendor_id == (uint32_t)vendor_id &&
                joints[i].product_code == (uint32_t)product_code) {
                print_error(file, (unsigned int)(joint_node->start_mark.line + 1),
                            (unsigned int)(joint_node->start_mark.column + 1),
                            "robot.joints[].identity",
                            "duplicate identity (position=%lu, vendor_id=0x%lX, product_code=0x%lX)",
                            position, vendor_id, product_code);
                return -1;
            }
        }

        joint = &joints[*joint_count];
        joint->name = strdup(joint_name);
        joint->group_enum = group_enum;
        joint->position = (uint16_t)position;
        joint->vendor_id = (uint32_t)vendor_id;
        joint->product_code = (uint32_t)product_code;
        joint->mark_line = (unsigned int)(joint_node->start_mark.line + 1);
        joint->mark_col = (unsigned int)(joint_node->start_mark.column + 1);
        (*joint_count)++;
    }

    if (*joint_count == 0) {
        print_error(file, (unsigned int)(joints_node->start_mark.line + 1),
                    (unsigned int)(joints_node->start_mark.column + 1),
                    "robot.joints", "must contain at least one joint");
        return -1;
    }

    return 0;
}

static int write_master_files(const char *interface,
                              const char *h_path, const char *c_path)
{
    FILE *h = fopen(h_path, "w");
    FILE *c = fopen(c_path, "w");

    if (!h || !c) {
        print_file_error(h_path, "failed to open output file");
        if (h) {
            fclose(h);
        }
        if (c) {
            fclose(c);
        }
        return -1;
    }

    fprintf(h, "/* Auto-generated by mo_ecat_config_gen from master.yaml */\n");
    fprintf(h, "/* DO NOT EDIT MANUALLY */\n");
    fprintf(h, "\n");
    fprintf(h, "#ifndef MO_ECAT_MASTER_CFG_H\n");
    fprintf(h, "#define MO_ECAT_MASTER_CFG_H\n");
    fprintf(h, "\n");
    fprintf(h, "#include \"config/master_config.h\"\n");
    fprintf(h, "\n");
    fprintf(h, "extern const struct mo_ecat_master_config g_master_config;\n");
    fprintf(h, "\n");
    fprintf(h, "#endif /* MO_ECAT_MASTER_CFG_H */\n");

    fprintf(c, "/* Auto-generated by mo_ecat_config_gen from master.yaml */\n");
    fprintf(c, "/* DO NOT EDIT MANUALLY */\n");
    fprintf(c, "\n");
    fprintf(c, "#include \"mo_ecat_master_cfg.h\"\n");
    fprintf(c, "\n");
    fprintf(c, "const struct mo_ecat_master_config g_master_config = {\n");
    fprintf(c, "    .interface_name = \"%s\",\n", interface);
    fprintf(c, "};\n");

    fclose(h);
    fclose(c);
    return 0;
}

static int write_robot_files(const char *robot_name,
                             const struct joint_entry *joints, size_t joint_count,
                             const char *h_path, const char *c_path)
{
    FILE *h = fopen(h_path, "w");
    FILE *c = fopen(c_path, "w");

    if (!h || !c) {
        print_file_error(h_path, "failed to open output file");
        if (h) {
            fclose(h);
        }
        if (c) {
            fclose(c);
        }
        return -1;
    }

    fprintf(h, "/* Auto-generated by mo_ecat_config_gen from robot.yaml */\n");
    fprintf(h, "/* DO NOT EDIT MANUALLY */\n");
    fprintf(h, "\n");
    fprintf(h, "#ifndef ROBOT_CFG_H\n");
    fprintf(h, "#define ROBOT_CFG_H\n");
    fprintf(h, "\n");
    fprintf(h, "#include \"robot_config.h\"\n");
    fprintf(h, "\n");
    fprintf(h, "extern const struct robot_config g_robot_config;\n");
    fprintf(h, "\n");
    fprintf(h, "#endif /* ROBOT_CFG_H */\n");

    fprintf(c, "/* Auto-generated by mo_ecat_config_gen from robot.yaml */\n");
    fprintf(c, "/* DO NOT EDIT MANUALLY */\n");
    fprintf(c, "\n");
    fprintf(c, "#include \"robot_cfg.h\"\n");
    fprintf(c, "\n");
    fprintf(c, "static const struct robot_joint_config s_joints[] = {\n");

    for (size_t i = 0; i < joint_count; ++i) {
        const struct joint_entry *joint = &joints[i];
        fprintf(c, "    {\n");
        fprintf(c, "        .joint_name = \"%s\",\n", joint->name);
        fprintf(c, "        .group      = %s,\n", joint->group_enum);
        fprintf(c, "        .identity   = {\n");
        fprintf(c, "            .position     = %u,\n", joint->position);
        fprintf(c, "            .vendor_id    = 0x%08X,\n", joint->vendor_id);
        fprintf(c, "            .product_code = 0x%08X,\n", joint->product_code);
        fprintf(c, "        },\n");
        fprintf(c, "    },\n");
    }

    fprintf(c, "};\n");
    fprintf(c, "\n");
    fprintf(c, "const struct robot_config g_robot_config = {\n");
    fprintf(c, "    .name        = \"%s\",\n", robot_name);
    fprintf(c, "    .joints      = s_joints,\n");
    fprintf(c, "    .joint_count = sizeof(s_joints) / sizeof(s_joints[0]),\n");
    fprintf(c, "};\n");

    fclose(h);
    fclose(c);
    return 0;
}

static int load_yaml(const char *file, yaml_document_t *doc)
{
    FILE *fp = fopen(file, "rb");
    yaml_parser_t parser;
    int rc;

    if (!fp) {
        print_file_error(file, "failed to open file");
        return -1;
    }

    if (!yaml_parser_initialize(&parser)) {
        print_file_error(file, "failed to initialize YAML parser");
        fclose(fp);
        return -1;
    }

    yaml_parser_set_input_file(&parser, fp);
    rc = yaml_parser_load(&parser, doc);
    yaml_parser_delete(&parser);
    fclose(fp);

    if (!rc) {
        print_file_error(file, "failed to parse YAML");
        return -1;
    }

    return 0;
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: %s --master-yaml <file> --robot-yaml <file> "
            "--master-out-h <file> --master-out-c <file> "
            "--robot-out-h <file> --robot-out-c <file>\n",
            prog_name);
}

int main(int argc, char *argv[])
{
    const char *master_yaml = NULL;
    const char *robot_yaml = NULL;
    const char *master_out_h = NULL;
    const char *master_out_c = NULL;
    const char *robot_out_h = NULL;
    const char *robot_out_c = NULL;
    yaml_document_t master_doc;
    yaml_document_t robot_doc;
    char interface[128];
    char robot_name[128];
    struct joint_entry joints[MAX_JOINTS];
    size_t joint_count = 0;
    int result = 0;

    prog_name = argv[0] ? argv[0] : "mo_ecat_config_gen";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--master-yaml") == 0 && i + 1 < argc) {
            master_yaml = argv[++i];
        } else if (strcmp(argv[i], "--robot-yaml") == 0 && i + 1 < argc) {
            robot_yaml = argv[++i];
        } else if (strcmp(argv[i], "--master-out-h") == 0 && i + 1 < argc) {
            master_out_h = argv[++i];
        } else if (strcmp(argv[i], "--master-out-c") == 0 && i + 1 < argc) {
            master_out_c = argv[++i];
        } else if (strcmp(argv[i], "--robot-out-h") == 0 && i + 1 < argc) {
            robot_out_h = argv[++i];
        } else if (strcmp(argv[i], "--robot-out-c") == 0 && i + 1 < argc) {
            robot_out_c = argv[++i];
        } else {
            usage();
            return 1;
        }
    }

    if (!master_yaml || !robot_yaml || !master_out_h || !master_out_c ||
        !robot_out_h || !robot_out_c) {
        usage();
        return 1;
    }

    if (load_yaml(master_yaml, &master_doc) < 0) {
        return 1;
    }

    if (load_yaml(robot_yaml, &robot_doc) < 0) {
        yaml_document_delete(&master_doc);
        return 1;
    }

    if (parse_master_yaml(master_yaml, &master_doc, interface, sizeof(interface)) < 0 ||
        parse_robot_yaml(robot_yaml, &robot_doc, robot_name, sizeof(robot_name),
                         joints, &joint_count) < 0) {
        result = 1;
        goto cleanup;
    }

    if (mkdir_for_file(master_out_h) < 0 || mkdir_for_file(master_out_c) < 0 ||
        mkdir_for_file(robot_out_h) < 0 || mkdir_for_file(robot_out_c) < 0) {
        fprintf(stderr, "[ERROR] failed to create output directories\n");
        result = 1;
        goto cleanup;
    }

    if (write_master_files(interface, master_out_h, master_out_c) < 0 ||
        write_robot_files(robot_name, joints, joint_count,
                          robot_out_h, robot_out_c) < 0) {
        result = 1;
        goto cleanup;
    }

    printf("Generated: %s, %s, %s, %s\n",
           master_out_h, master_out_c, robot_out_h, robot_out_c);

cleanup:
    yaml_document_delete(&master_doc);
    yaml_document_delete(&robot_doc);
    for (size_t i = 0; i < joint_count; ++i) {
        free((void *)joints[i].name);
    }

    return result;
}
