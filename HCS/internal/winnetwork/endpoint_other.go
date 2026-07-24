//go:build !windows

package winnetwork

import "fmt"

type Endpoint struct{}

func CreateEndpoint(networkName string) (*Endpoint, error) {
	return nil, fmt.Errorf("HCN mreža %q je podržana samo na Windows hostu", networkName)
}

func (e *Endpoint) ID() string {
	return ""
}

func (e *Endpoint) Delete() error {
	return nil
}
