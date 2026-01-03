const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 1. Create the root module first with target and optimize settings
    const exe_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    // 2. Add your C sources and includes directly to this module
    exe_mod.addIncludePath(b.path("include"));
    exe_mod.addCSourceFiles(.{
        .files = &.{
            "src/khmer_segmenter.c",
            "src/main.c",
            "src/re.c",
            "src/khmer_normalization.c",
            "src/khmer_rule_engine.c",
        },
        .flags = &.{ "-Wall", "-Wextra", "-std=c11" },
    });

    // 3. Create the executable using that module
    const exe = b.addExecutable(.{
        .name = "khmer_segmenter",
        .root_module = exe_mod,
    });

    // This helper still works on the artifact
    exe.linkLibC();
    exe.linkSystemLibrary("psapi");

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_cmd.step);
}
