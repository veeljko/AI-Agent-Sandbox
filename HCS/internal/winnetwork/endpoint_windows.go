//go:build windows

package winnetwork

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/Microsoft/hcsshim/hcn"
)

type Endpoint struct {
	endpoint *hcn.HostComputeEndpoint
}

func CreateEndpoint(networkName string) (*Endpoint, error) {
	network, err := hcn.GetNetworkByName(networkName)
	if err != nil {
		return nil, fmt.Errorf("HCN mreža %q nije dostupna: %w", networkName, err)
	}

	name, err := uniqueEndpointName()
	if err != nil {
		return nil, err
	}
	endpoint, err := network.CreateEndpoint(&hcn.HostComputeEndpoint{
		Name: name,
		SchemaVersion: hcn.SchemaVersion{
			Major: 2,
			Minor: 0,
		},
	})
	if err != nil {
		return nil, fmt.Errorf("kreiranje HCN endpointa na mreži %q: %w", networkName, err)
	}
	return &Endpoint{endpoint: endpoint}, nil
}

func (e *Endpoint) ID() string {
	if e == nil || e.endpoint == nil {
		return ""
	}
	return e.endpoint.Id
}

func (e *Endpoint) Delete() error {
	if e == nil || e.endpoint == nil {
		return nil
	}
	if err := e.endpoint.Delete(); err != nil && !hcn.IsElementNotFoundError(err) {
		return fmt.Errorf("brisanje HCN endpointa %s: %w", e.endpoint.Id, err)
	}
	e.endpoint = nil
	return nil
}

func uniqueEndpointName() (string, error) {
	random := make([]byte, 6)
	if _, err := rand.Read(random); err != nil {
		return "", fmt.Errorf("generisanje HCN endpoint imena: %w", err)
	}
	return fmt.Sprintf("codex-%d-%s", time.Now().UTC().UnixMilli(), hex.EncodeToString(random)), nil
}
