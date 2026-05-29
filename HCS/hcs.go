package main

import (
	"bufio"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"

	hcsshim "github.com/Microsoft/hcsshim"
	"golang.org/x/sys/windows"
)

const (
	defaultContainerID      = ""
	defaultOwner            = "ai-agent-sandbox"
	defaultStorageRoot      = `C:\ProgramData\ai-agent-sandbox\windowsfilter`
	defaultHostWorkdir      = `C:\Users\Korisnik\OneDrive\Desktop\test`
	defaultContainerWorkdir = `C:\workspace`
	defaultShell            = `C:\Windows\System32\cmd.exe`
)

type layerFlag []string

func (l *layerFlag) String() string {
	return strings.Join(*l, ";")
}

func (l *layerFlag) Set(value string) error {
	value = strings.TrimSpace(value)
	if value == "" {
		return errors.New("layer path must not be empty")
	}
	*l = append(*l, value)
	return nil
}

type stringListFlag []string

func (s *stringListFlag) String() string {
	return strings.Join(*s, ";")
}

func (s *stringListFlag) Set(value string) error {
	value = strings.TrimSpace(value)
	if value == "" {
		return errors.New("value must not be empty")
	}
	*s = append(*s, value)
	return nil
}

type options struct {
	containerID      string
	owner            string
	storageRoot      string
	hostWorkdir      string
	containerWorkdir string
	networkName      string
	dnsServers       string
	memoryMB         int64
	cpuCount         uint
	cleanup          bool
	emulateConsole   bool
	stripANSI        bool
	forwardStdin     bool
	layerSpecs       []string
	mountSpecs       []string
	readOnlyMounts   []string
	envSpecs         []string
	command          []string
}

type parentLayer struct {
	ID   string
	Path string
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}
}

func run() error {
	opts := parseFlags()

	_ = enableVirtualTerminalOutput(os.Stdout)
	_ = enableVirtualTerminalOutput(os.Stderr)
	_ = enableVirtualTerminalInput(os.Stdin)

	if len(opts.layerSpecs) == 0 {
		return fmt.Errorf("missing Windows image layers; pass -layer <top-layer-folder>; Docker windowsfilter layerchain.json is expanded automatically when present")
	}

	hostWorkdir, err := filepath.Abs(opts.hostWorkdir)
	if err != nil {
		return fmt.Errorf("resolve host workdir: %w", err)
	}
	if info, err := os.Stat(hostWorkdir); err != nil {
		return fmt.Errorf("host workdir not found at %s: %w", hostWorkdir, err)
	} else if !info.IsDir() {
		return fmt.Errorf("host workdir is not a directory: %s", hostWorkdir)
	}

	if err := os.MkdirAll(opts.storageRoot, 0o755); err != nil {
		return fmt.Errorf("create storage root %s: %w", opts.storageRoot, err)
	}

	layerSpecs, err := expandDockerLayerChain(opts.layerSpecs)
	if err != nil {
		return err
	}

	parentLayers, parentLayerPaths, err := resolveParentLayers(layerSpecs)
	if err != nil {
		return err
	}

	driverInfo := hcsshim.DriverInfo{HomeDir: opts.storageRoot}
	if err := cleanupContainerStorage(opts.containerID, driverInfo); err != nil {
		return err
	}

	if err := hcsshim.CreateScratchLayer(driverInfo, opts.containerID, parentLayers[0].ID, parentLayerPaths); err != nil {
		return fmt.Errorf("create scratch layer: %w", err)
	}
	layerPrepared := false
	layerActivated := false
	defer func() {
		if !opts.cleanup {
			return
		}
		if layerPrepared {
			_ = hcsshim.UnprepareLayer(driverInfo, opts.containerID)
		}
		if layerActivated {
			_ = hcsshim.DeactivateLayer(driverInfo, opts.containerID)
		}
		_ = hcsshim.DestroyLayer(driverInfo, opts.containerID)
	}()

	if err := hcsshim.ActivateLayer(driverInfo, opts.containerID); err != nil {
		return fmt.Errorf("activate scratch layer: %w", err)
	}
	layerActivated = true

	if err := hcsshim.PrepareLayer(driverInfo, opts.containerID, parentLayerPaths); err != nil {
		return fmt.Errorf("prepare scratch layer: %w", err)
	}
	layerPrepared = true

	volumePath, err := hcsshim.GetLayerMountPath(driverInfo, opts.containerID)
	if err != nil {
		return fmt.Errorf("get scratch volume path: %w", err)
	}

	endpoint, err := createHNSEndpoint(opts)
	if err != nil {
		return err
	}
	if endpoint != nil {
		defer endpoint.Delete()
	}

	container, err := hcsshim.CreateContainer(opts.containerID, buildContainerConfig(opts, parentLayers, volumePath, hostWorkdir, endpoint))
	if err != nil {
		return fmt.Errorf("create container: %w", err)
	}
	defer container.Close()
	defer func() {
		_ = container.Terminate()
		_ = container.WaitTimeout(5 * time.Second)
	}()

	if err := container.Start(); err != nil {
		return fmt.Errorf("start container: %w", err)
	}

	processConfig := buildProcessConfig(opts)
	fmt.Printf("HCS sandbox started as %s. Host folder %s is mounted as %s.\n", opts.containerID, hostWorkdir, opts.containerWorkdir)
	fmt.Printf("Running: %s\n\n", processConfig.CommandLine)

	process, err := container.CreateProcess(processConfig)
	if err != nil {
		return fmt.Errorf("create process: %w", err)
	}
	defer process.Close()

	stdin, stdout, stderr, err := process.Stdio()
	if err != nil {
		return fmt.Errorf("open process stdio: %w", err)
	}
	defer stdin.Close()

	var outputWG sync.WaitGroup
	outputWG.Add(2)
	go copyOutput(&outputWG, os.Stdout, stdout, opts.stripANSI)
	go copyOutput(&outputWG, os.Stderr, stderr, opts.stripANSI)
	if opts.forwardStdin {
		go func() {
			_, _ = io.Copy(stdin, os.Stdin)
			_ = process.CloseStdin()
		}()
	} else {
		_ = process.CloseStdin()
	}

	if err := process.Wait(); err != nil {
		return fmt.Errorf("wait process: %w", err)
	}
	outputWG.Wait()

	exitCode, err := process.ExitCode()
	if err != nil {
		return fmt.Errorf("get process exit code: %w", err)
	}
	if exitCode != 0 {
		return fmt.Errorf("sandboxed process exited with code %d", exitCode)
	}

	return nil
}

func parseFlags() options {
	var layers layerFlag
	var layersCSV string
	var mounts stringListFlag
	var readOnlyMounts stringListFlag
	var envs stringListFlag

	opts := options{}
	flag.StringVar(&opts.containerID, "id", defaultContainerID, "HCS container ID; defaults to a unique jit-sandbox-* ID")
	flag.StringVar(&opts.owner, "owner", defaultOwner, "HCS owner name")
	flag.StringVar(&opts.storageRoot, "storage-root", defaultStorageRoot, "root folder for hcsshim scratch layers")
	flag.StringVar(&opts.hostWorkdir, "workdir", defaultHostWorkdir, "host folder allowed inside the sandbox")
	flag.StringVar(&opts.containerWorkdir, "container-workdir", defaultContainerWorkdir, "mount path for workdir inside the container")
	flag.StringVar(&opts.networkName, "network", "nat", "HNS network name to attach; empty disables networking")
	flag.StringVar(&opts.dnsServers, "dns", "1.1.1.1,8.8.8.8", "comma-separated DNS servers for the HNS endpoint")
	flag.Int64Var(&opts.memoryMB, "memory-mb", 0, "optional container memory limit in MB")
	flag.UintVar(&opts.cpuCount, "cpu-count", 0, "optional container CPU count")
	flag.BoolVar(&opts.cleanup, "cleanup", true, "remove scratch layer after the process exits")
	flag.BoolVar(&opts.emulateConsole, "emulate-console", true, "ask HCS to emulate a console for the process")
	flag.BoolVar(&opts.stripANSI, "strip-ansi", false, "strip ANSI escape sequences from process output")
	flag.BoolVar(&opts.forwardStdin, "stdin", true, "forward host stdin to the sandboxed process")
	flag.Var(&layers, "layer", "Windows image layer folder, optionally ID=PATH; repeat from top image layer to base layer, or pass only the top Docker windowsfilter layer")
	flag.StringVar(&layersCSV, "layers", "", "semicolon-separated Windows image layer folders, optionally ID=PATH, ordered from top image layer to base layer")
	flag.Var(&mounts, "mount", "extra directory mount in HOST=CONTAINER form; can be repeated")
	flag.Var(&readOnlyMounts, "mount-ro", "extra read-only directory mount in HOST=CONTAINER form; can be repeated")
	flag.Var(&envs, "env", "environment variable in KEY=VALUE form; can be repeated")
	flag.Parse()

	if strings.TrimSpace(opts.containerID) == "" {
		opts.containerID = newContainerID()
	}

	if layersCSV != "" {
		for _, part := range strings.Split(layersCSV, ";") {
			part = strings.TrimSpace(part)
			if part != "" {
				layers = append(layers, part)
			}
		}
	}

	opts.layerSpecs = layers
	opts.mountSpecs = mounts
	opts.readOnlyMounts = readOnlyMounts
	opts.envSpecs = envs
	opts.command = flag.Args()
	return opts
}

func newContainerID() string {
	return fmt.Sprintf("jit-sandbox-%d-%d", time.Now().UnixNano(), os.Getpid())
}

func resolveParentLayers(layerSpecs []string) ([]parentLayer, []string, error) {
	parentLayers := make([]parentLayer, 0, len(layerSpecs))
	parentLayerPaths := make([]string, 0, len(layerSpecs))

	for _, spec := range layerSpecs {
		id, path := splitLayerSpec(spec)
		absPath, err := filepath.Abs(path)
		if err != nil {
			return nil, nil, fmt.Errorf("resolve layer path %s: %w", path, err)
		}
		if info, err := os.Stat(absPath); err != nil {
			return nil, nil, fmt.Errorf("layer path not found %s: %w", absPath, err)
		} else if !info.IsDir() {
			return nil, nil, fmt.Errorf("layer path is not a directory: %s", absPath)
		}
		if id == "" {
			guid, err := hcsshim.NameToGuid(filepath.Base(absPath))
			if err != nil {
				return nil, nil, fmt.Errorf("derive layer ID for %s: %w", absPath, err)
			}
			id = guid.ToString()
		}
		parentLayers = append(parentLayers, parentLayer{ID: id, Path: absPath})
		parentLayerPaths = append(parentLayerPaths, absPath)
	}

	return parentLayers, parentLayerPaths, nil
}

func expandDockerLayerChain(layerSpecs []string) ([]string, error) {
	if len(layerSpecs) != 1 {
		return layerSpecs, nil
	}

	id, path := splitLayerSpec(layerSpecs[0])
	absPath, err := filepath.Abs(path)
	if err != nil {
		return nil, fmt.Errorf("resolve layer path %s: %w", path, err)
	}

	content, err := os.ReadFile(filepath.Join(absPath, "layerchain.json"))
	if errors.Is(err, os.ErrNotExist) {
		return layerSpecs, nil
	}
	if err != nil {
		return nil, fmt.Errorf("read layerchain.json from %s: %w", absPath, err)
	}

	var parentPaths []string
	if err := json.Unmarshal(content, &parentPaths); err != nil {
		return nil, fmt.Errorf("parse layerchain.json from %s: %w", absPath, err)
	}

	topSpec := absPath
	if id != "" {
		topSpec = id + "=" + absPath
	}

	expanded := []string{topSpec}
	expanded = append(expanded, parentPaths...)
	return expanded, nil
}

func splitLayerSpec(spec string) (string, string) {
	id, path, found := strings.Cut(spec, "=")
	if !found {
		return "", spec
	}
	return strings.TrimSpace(id), strings.TrimSpace(path)
}

func cleanupContainerStorage(containerID string, driverInfo hcsshim.DriverInfo) error {
	if container, err := hcsshim.OpenContainer(containerID); err == nil {
		_ = container.Terminate()
		_ = container.WaitTimeout(5 * time.Second)
		_ = container.Close()
	}

	_ = hcsshim.UnprepareLayer(driverInfo, containerID)
	_ = hcsshim.DeactivateLayer(driverInfo, containerID)
	if exists, err := hcsshim.LayerExists(driverInfo, containerID); err == nil && exists {
		if err := hcsshim.DestroyLayer(driverInfo, containerID); err != nil {
			return fmt.Errorf("destroy old scratch layer %s: %w", containerID, err)
		}
	}
	return nil
}

func createHNSEndpoint(opts options) (*hcsshim.HNSEndpoint, error) {
	if strings.TrimSpace(opts.networkName) == "" {
		return nil, nil
	}

	network, err := hcsshim.GetHNSNetworkByName(opts.networkName)
	if err != nil {
		return nil, fmt.Errorf("get HNS network %q: %w", opts.networkName, err)
	}

	endpointName := opts.containerID + "-endpoint"
	if oldEndpoint, err := hcsshim.GetHNSEndpointByName(endpointName); err == nil {
		_, _ = oldEndpoint.Delete()
	}

	endpoint := &hcsshim.HNSEndpoint{
		Name:               endpointName,
		VirtualNetwork:     network.Id,
		VirtualNetworkName: network.Name,
		DNSServerList:      opts.dnsServers,
		EnableInternalDNS:  true,
	}

	createdEndpoint, err := network.CreateEndpoint(endpoint)
	if err != nil {
		return nil, fmt.Errorf("create HNS endpoint on network %q: %w", opts.networkName, err)
	}
	return createdEndpoint, nil
}

func buildContainerConfig(opts options, parentLayers []parentLayer, volumePath string, hostWorkdir string, endpoint *hcsshim.HNSEndpoint) *hcsshim.ContainerConfig {
	layers := make([]hcsshim.Layer, 0, len(parentLayers))
	for _, layer := range parentLayers {
		layers = append(layers, hcsshim.Layer{
			ID:   layer.ID,
			Path: layer.Path,
		})
	}

	mappedDirectories := []hcsshim.MappedDir{
		{
			HostPath:      hostWorkdir,
			ContainerPath: opts.containerWorkdir,
			ReadOnly:      false,
		},
	}
	for _, spec := range opts.mountSpecs {
		hostPath, containerPath, err := splitKeyValue(spec)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ignoring invalid mount %q: %v\n", spec, err)
			continue
		}
		mappedDirectories = append(mappedDirectories, hcsshim.MappedDir{
			HostPath:      hostPath,
			ContainerPath: containerPath,
			ReadOnly:      false,
		})
	}
	for _, spec := range opts.readOnlyMounts {
		hostPath, containerPath, err := splitKeyValue(spec)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ignoring invalid read-only mount %q: %v\n", spec, err)
			continue
		}
		mappedDirectories = append(mappedDirectories, hcsshim.MappedDir{
			HostPath:      hostPath,
			ContainerPath: containerPath,
			ReadOnly:      true,
		})
	}

	config := &hcsshim.ContainerConfig{
		SystemType:               "Container",
		Name:                     opts.containerID,
		Owner:                    opts.owner,
		VolumePath:               strings.TrimRight(volumePath, `\`),
		LayerFolderPath:          filepath.Join(opts.storageRoot, opts.containerID),
		Layers:                   layers,
		HostName:                 opts.containerID,
		HvPartition:              false,
		MappedDirectories:        mappedDirectories,
		AllowUnqualifiedDNSQuery: true,
		IgnoreFlushesDuringBoot:  true,
	}

	if endpoint != nil {
		config.EndpointList = []string{endpoint.Id}
	}

	if opts.memoryMB > 0 {
		config.MemoryMaximumInMB = opts.memoryMB
	}
	if opts.cpuCount > 0 {
		config.ProcessorCount = uint32(opts.cpuCount)
	}

	return config
}

func buildProcessConfig(opts options) *hcsshim.ProcessConfig {
	command := opts.command
	if len(command) == 0 {
		command = []string{
			defaultShell,
			"/Q",
			"/K",
			`echo HCS sandbox ready. Only C:\workspace is mounted from the host. Type exit to close. && prompt [hcs]$G`,
		}
	}

	appPath := command[0]
	environment := map[string]string{
		"AI_AGENT_SANDBOX": "hcs",
		"SANDBOX_WORKDIR":  opts.containerWorkdir,
	}
	addHostEnvironment(environment, "OPENAI_API_KEY")
	addHostEnvironment(environment, "OPENAI_BASE_URL")
	addHostEnvironment(environment, "OPENAI_ORG_ID")
	addHostEnvironment(environment, "OPENAI_PROJECT_ID")
	addHostEnvironment(environment, "HTTPS_PROXY")
	addHostEnvironment(environment, "HTTP_PROXY")
	addHostEnvironment(environment, "NO_PROXY")

	for _, spec := range opts.envSpecs {
		name, value, err := splitKeyValue(spec)
		if err != nil {
			fmt.Fprintf(os.Stderr, "ignoring invalid env %q: %v\n", spec, err)
			continue
		}
		environment[name] = value
	}

	return &hcsshim.ProcessConfig{
		ApplicationName:  appPath,
		CommandLine:      windowsCommandLine(command),
		WorkingDirectory: opts.containerWorkdir,
		Environment:      environment,
		EmulateConsole:   opts.emulateConsole,
		CreateStdInPipe:  true,
		CreateStdOutPipe: true,
		CreateStdErrPipe: true,
		ConsoleSize:      [2]uint{120, 30},
	}
}

func addHostEnvironment(environment map[string]string, name string) {
	if value := os.Getenv(name); value != "" {
		environment[name] = value
	}
}

func windowsCommandLine(args []string) string {
	quoted := make([]string, 0, len(args))
	for _, arg := range args {
		quoted = append(quoted, quoteWindowsArg(arg))
	}
	return strings.Join(quoted, " ")
}

func quoteWindowsArg(value string) string {
	if value == "" {
		return `""`
	}
	if strings.ContainsAny(value, " \t\"&|<>") {
		return `"` + strings.ReplaceAll(value, `"`, `\"`) + `"`
	}
	return value
}

func splitKeyValue(spec string) (string, string, error) {
	key, value, found := strings.Cut(spec, "=")
	if !found {
		return "", "", fmt.Errorf("expected KEY=VALUE")
	}
	key = strings.TrimSpace(key)
	value = strings.TrimSpace(value)
	if key == "" || value == "" {
		return "", "", fmt.Errorf("key and value must not be empty")
	}
	return key, value, nil
}

func copyOutput(wg *sync.WaitGroup, dst io.Writer, src io.Reader, stripANSI bool) {
	defer wg.Done()
	if !stripANSI {
		_, _ = io.Copy(dst, src)
		return
	}

	scanner := bufio.NewScanner(src)
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	ansiPattern := regexp.MustCompile(`\x1b\[[0-?]*[ -/]*[@-~]|\x1b\][^\a]*(\a|\x1b\\)`)
	for scanner.Scan() {
		line := ansiPattern.ReplaceAllString(scanner.Text(), "")
		_, _ = fmt.Fprintln(dst, line)
	}
}

func enableVirtualTerminalOutput(file *os.File) error {
	const enableVirtualTerminalProcessing uint32 = 0x0004

	handle := windows.Handle(file.Fd())
	var mode uint32
	if err := windows.GetConsoleMode(handle, &mode); err != nil {
		return nil
	}

	return windows.SetConsoleMode(handle, mode|enableVirtualTerminalProcessing)
}

func enableVirtualTerminalInput(file *os.File) error {
	const enableVirtualTerminalInput uint32 = 0x0200

	handle := windows.Handle(file.Fd())
	var mode uint32
	if err := windows.GetConsoleMode(handle, &mode); err != nil {
		return nil
	}

	return windows.SetConsoleMode(handle, mode|enableVirtualTerminalInput)
}
